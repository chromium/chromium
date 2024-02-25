// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When the installer runs, it enumerates its own resources with type "SCRIPT".
// Resources with the known name ("BATCH", "POWERSHELL" or "PYTHON") are
// extracted to a temp folder and executed with the matching interpreter.
// All command lines arguments are forwarded to the child process.

#include <windows.h>

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "chrome/updater/win/installer/pe_resource.h"

namespace {

base::CommandLine::StringType ExtensionFromResourceName(
    const std::wstring& name) {
  if (name == L"BATCH") {
    return FILE_PATH_LITERAL(".cmd");
  } else if (name == L"POWERSHELL") {
    return FILE_PATH_LITERAL(".ps1");
  } else if (name == L"PYTHON") {
    return FILE_PATH_LITERAL(".py");
  }

  return {};
}

base::CommandLine::StringType CommandWrapperForScript(
    const base::FilePath& script_path) {
  const base::FilePath::StringType extension = script_path.Extension();
  if (extension == FILE_PATH_LITERAL(".ps1")) {
    return FILE_PATH_LITERAL("powershell.exe");
  }
  if (extension == FILE_PATH_LITERAL(".py")) {
    return FILE_PATH_LITERAL("vpython3.bat");
  }

  return {};
}

std::optional<int> RunScript(const base::FilePath& script_path) {
  // Copy current process's command line so all arguments are forwarded.
  base::CommandLine command = *base::CommandLine::ForCurrentProcess();
  command.SetProgram(script_path);
  command.PrependWrapper(CommandWrapperForScript(script_path));
  int exit_code = -1;
  return base::LaunchProcess(command, {})
                 .WaitForExitWithTimeout(base::Minutes(1), &exit_code)
             ? std::make_optional(exit_code)
             : std::nullopt;
}

std::optional<base::FilePath> CreateScriptFile(
    HMODULE module,
    const std::wstring& name,
    const std::wstring& type,
    const base::FilePath& working_dir) {
  CHECK(name == L"BATCH" || name == L"POWERSHELL" || name == L"PYTHON");

  updater::PEResource resource(name.c_str(), type.c_str(), module);
  if (!resource.IsValid() || resource.Size() < 1) {
    return {};
  }

  const base::FilePath script_path =
      working_dir.AppendASCII("TestAppSetup")
          .AddExtension(ExtensionFromResourceName(name));
  return resource.WriteToDisk(script_path.value().c_str())
             ? std::make_optional(script_path)
             : std::nullopt;
}

BOOL CALLBACK OnResourceFound(HMODULE module,
                              const wchar_t* type,
                              wchar_t* name,
                              LONG_PTR context) {
  CHECK(type);
  CHECK(name);
  if (!context) {
    return FALSE;
  }

  const std::wstring resource_name(name);
  if (resource_name != L"BATCH" && resource_name != L"POWERSHELL" &&
      resource_name != L"PYTHON") {
    // Ignore unsupported script type and continue to enumerate other resources.
    return TRUE;
  }
  const base::FilePath* working_dir =
      reinterpret_cast<const base::FilePath*>(context);
  const auto script_path = CreateScriptFile(module, name, type, *working_dir);
  return script_path && RunScript(*script_path);
}

int RunAllResourceScripts() {
  constexpr char kScriptResourceNameSwitch[] = "script_resource_name";

  base::ScopedTempDir working_dir;
  if (!working_dir.CreateUniqueTempDir()) {
    return 1;
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kScriptResourceNameSwitch)) {
    return ::EnumResourceNames(
               nullptr, L"SCRIPT", OnResourceFound,
               reinterpret_cast<LONG_PTR>(&working_dir.GetPath()))
               ? 0
               : 1;
  }

  const auto script_resource_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kScriptResourceNameSwitch);
  const auto script_path = CreateScriptFile(nullptr, script_resource_name,
                                            L"SCRIPT", working_dir.GetPath());
  if (!script_path) {
    return -1;
  }
  const auto result = RunScript(*script_path);
  return result ? *result : -1;
}

}  // namespace

int main(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  return RunAllResourceScripts();
}
