// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When the installer runs, it enumerates its own resources with type "SCRIPT".
// Resources with the known name ("BATCH", "POWERSHELL" or "PYTHON") are
// extracted to a temp folder and executed with the matching interpreter.
// All command lines arguments are forwarded to the child process.

#include <windows.h>
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

bool RunScript(const base::FilePath& script_path) {
  // Copy current process's command line so all arguments are forwarded.
  base::CommandLine command = *base::CommandLine::ForCurrentProcess();
  command.SetProgram(script_path);
  command.PrependWrapper(CommandWrapperForScript(script_path));
  int exit_code = -1;
  return base::LaunchProcess(command, {})
             .WaitForExitWithTimeout(base::Minutes(1), &exit_code) &&
         exit_code == 0;
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

  updater::PEResource resource(name, type, module);
  if (!resource.IsValid() || resource.Size() < 1) {
    return FALSE;
  }

  const base::FilePath* working_dir =
      reinterpret_cast<const base::FilePath*>(context);
  const base::FilePath script_path =
      working_dir->AppendASCII("TestAppSetup")
          .AddExtension(ExtensionFromResourceName(name));
  if (!resource.WriteToDisk(script_path.value().c_str()) ||
      !RunScript(script_path)) {
    return FALSE;
  }

  return TRUE;
}

int RunAllResourceScripts() {
  base::ScopedTempDir working_dir;
  if (!working_dir.CreateUniqueTempDir()) {
    return 1;
  }

  return ::EnumResourceNames(nullptr, L"SCRIPT", OnResourceFound,
                             reinterpret_cast<LONG_PTR>(&working_dir.GetPath()))
             ? 0
             : 1;
}

}  // namespace

int main(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  return RunAllResourceScripts();
}
