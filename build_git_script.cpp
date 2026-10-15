// Builds git_script's plain C++ executables (git_repo, git, git_repo_submodule,
// git_build) using build_helper (https://github.com/water-chika/build_helper,
// submodule at ./build_helper) instead of CMake.
//
// git_script's CMakeLists.txt uses target_compile_definitions to bake
// absolute paths (to its own .py helper scripts, to the real git binary, to
// python, and a platform path-list separator) into each executable.
// build::c_plus_plus_compile's source classification (directory -> -I,
// .so -> -l, .cpp -> as-is, -l/-L passthrough) has no way to pass a raw
// "-D..." flag, so instead of fighting that, each target here is built the
// same way build_librfb.cpp built rfb_window_demo/test_hip: compile_object()
// (which takes a free-form extra_flags vector, including -D defines) for the
// single source, then link_objects() to produce the executable.
#include <filesystem>
#include <iostream>
#include <vector>
#include "build.hpp"

int main() {
    std::filesystem::path root = ".";
    std::filesystem::path git_script = root / "git_script";
    // Separate build directory (CMake-style out-of-source build): every
    // compiled object/binary from this driver lands under here instead of
    // the source tree; build_helper creates it for us.
    std::filesystem::path build_dir = root / "build_git_script_out";

    auto host_cxx = build::find_cxx_compiler();
    auto git_repo_py = (git_script / "git_repo.py").string();
    auto git_submodule_py = (git_script / "git_submodule.py").string();
    auto git_build_py = (git_script / "git_build.py").string();
    std::string git_path = "/usr/bin/git";
    std::string python_path = "/usr/bin/python";

    struct target {
        std::string name;
        std::filesystem::path src;
        std::vector<std::string> defines;
    };

    // compile_object() feeds its flags verbatim to system()/the shell, so a
    // plain "\"" here would be consumed (stripped) by the shell before the
    // compiler ever sees it, leaving an unquoted path the preprocessor can't
    // parse as a string literal. Escaping as "\\\"" keeps a literal
    // backslash-quote in the flag; the shell then strips only the
    // backslash, delivering a real '"' to the compiler argv - exactly like
    // writing -DFOO=\"bar\" by hand on a command line.
    std::vector<target> targets = {
        {"git_repo", git_script / "git_repo.cpp",
         {"-DGIT_REPO_PY_PATH=\\\"" + git_repo_py + "\\\"",
          "-DGIT_REPO_SUBMODULE_PY_PATH=\\\"" + git_submodule_py + "\\\""}},
        {"git", git_script / "git.cpp",
         {"-DGIT_REPO_PY_PATH=\\\"" + git_repo_py + "\\\"",
          "-DGIT_REPO_SUBMODULE_PY_PATH=\\\"" + git_submodule_py + "\\\"",
          "-DPYTHON_PATH=\\\"" + python_path + "\\\"",
          "-DGIT_PATH=\\\"" + git_path + "\\\"",
          "-DPATH_SEPERATOR=':'"}},
        {"git_repo_submodule", git_script / "git_repo_submodule.cpp",
         {"-DGIT_REPO_SUBMODULE_PY_PATH=\\\"" + git_submodule_py + "\\\""}},
        {"git_build", git_script / "git_build.cpp",
         {"-DGIT_BUILD_PY_PATH=\\\"" + git_build_py + "\\\""}},
    };

    bool all_ok = true;
    for (auto& t : targets) {
        auto flags = t.defines;
        flags.push_back("-std=c++20");
        auto obj = build_dir / (t.name + ".o");
        auto out = (build_dir / (t.name + "_bh")).string();
        auto compile_res = build::compile_object(host_cxx, flags, t.src, obj);
        if (compile_res != 0) {
            std::cerr << "build_git_script: FAILED " << t.name << " (compile)" << std::endl;
            all_ok = false;
            continue;
        }
        auto link_res = build::link_objects(host_cxx, {obj.string()}, out);
        if (link_res == 0) {
            std::cout << "build_git_script: built " << out << std::endl;
        } else {
            std::cerr << "build_git_script: FAILED " << t.name << " (link)" << std::endl;
            all_ok = false;
        }
    }

    return all_ok ? 0 : 1;
}
