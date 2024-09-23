// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/chrome_elf/third_party_dlls/main_unittest_exe.h"

#include <windows.h>

#include <shellapi.h>
#include <stdlib.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/chrome_elf/third_party_dlls/main.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_file.h"
#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/product_install_details.h"

namespace {

// Function object which invokes LocalFree on its parameter, which must be
// a pointer.  To be used with std::unique_ptr and CommandLineToArgvW().
struct LocalFreeDeleter {
  inline void operator()(wchar_t** ptr) const { ::LocalFree(ptr); }
};

// Attempt to load a given DLL.
third_party_dlls::ExitCode LoadDll(std::wstring name) {
  base::FilePath dll_path(name);
  base::ScopedNativeLibrary dll(dll_path);
  return dll.is_valid() ? third_party_dlls::kDllLoadSuccess
                        : third_party_dlls::kDllLoadFailed;
}

// Utility function to protect the local registry.
void RegRedirect(registry_util::RegistryOverrideManager* rom) {
  std::wstring temp;
  rom->OverrideRegistry(HKEY_CURRENT_USER, &temp);
  nt::SetTestingOverride(nt::HKCU, temp);
}

// Compare an argument path with a module-load log path.
// - |arg_path| is a UTF-16 drive path.
// - |log.section_path| is UTF-8, and will be a device path, so convert to drive
//   letter before comparing.
bool MatchPath(const wchar_t* arg_path, const third_party_dlls::LogEntry& log) {
  base::FilePath drive_path;
  if (!base::DevicePathToDriveLetterPath(
          base::FilePath(base::UTF8ToWide(log.path)), &drive_path)) {
    return false;
  }

  return drive_path.value().compare(arg_path) == 0;
}

}  // namespace

//------------------------------------------------------------------------------
// PUBLIC
//------------------------------------------------------------------------------

// Good ol' main.
// - Init third_party_dlls, which will apply a hook to NtMapViewOfSection.
// - Attempt to load a specific DLL.
//
// Arguments:
// #1: path to test blocklist file (mandatory).
// #2: test identifier (mandatory).
// #3: path to dll (test-identifier dependent).
//
// Returns:
// - Negative values in case of unexpected error.
// - 0 for successful DLL load.
// - 1 for failed DLL load.
int main() {
  // NOTE: The arguments must be treated as unicode for these tests.
  int argument_count = 0;
  std::unique_ptr<wchar_t*[], LocalFreeDeleter> argv(
      ::CommandLineToArgvW(::GetCommandLineW(), &argument_count));
  if (!argv)
    return third_party_dlls::kBadCommandLine;

  if (IsThirdPartyInitialized())
    return third_party_dlls::kThirdPartyAlreadyInitialized;

  install_static::InitializeProductDetailsForPrimaryModule();
  install_static::InitializeProcessType();

  // Get the required arguments, path to blocklist file and test id to run.
  if (argument_count < 3)
    return third_party_dlls::kMissingArgument;

  const wchar_t* blocklist_path = argv[1];
  if (!blocklist_path || ::wcslen(blocklist_path) == 0)
    return third_party_dlls::kBadBlocklistPath;

  const wchar_t* arg2 = argv[2];
  int test_id = ::_wtoi(arg2);
  if (!test_id)
    return third_party_dlls::kUnsupportedTestId;

  // Override blocklist path before initializing.
  third_party_dlls::OverrideFilePathForTesting(blocklist_path);

  // Enable a registry test net before initializing.
  registry_util::RegistryOverrideManager rom;
  RegRedirect(&rom);

  if (!third_party_dlls::Init())
    return third_party_dlls::kThirdPartyInitFailure;

  switch (test_id) {
    case third_party_dlls::kTestOnlyInitialization:
      break;

    case third_party_dlls::kTestSingleDllLoad:
    case third_party_dlls::kTestLogPath: {
      if (argument_count < 4)
        return third_party_dlls::kMissingArgument;
      const wchar_t* dll_name = argv[3];
      if (!dll_name || ::wcslen(dll_name) == 0)
        return third_party_dlls::kBadArgument;
      third_party_dlls::ExitCode code = LoadDll(dll_name);

      // Get logging.  Ensure the log is as expected.
      uint32_t bytes = 0;
      DrainLog(nullptr, 0, &bytes);
      if (!bytes)
        return third_party_dlls::kEmptyLog;
      auto buffer = std::make_unique<uint8_t[]>(bytes);
      bytes = DrainLog(&buffer[0], bytes, nullptr);
      third_party_dlls::LogEntry* entry =
          reinterpret_cast<third_party_dlls::LogEntry*>(&buffer[0]);
      if (!bytes || bytes < third_party_dlls::GetLogEntrySize(entry->path_len))
        return third_party_dlls::kBadLogEntrySize;

      if ((code == third_party_dlls::kDllLoadFailed &&
           entry->type != third_party_dlls::kBlocked) ||
          (code == third_party_dlls::kDllLoadSuccess &&
           entry->type != third_party_dlls::kAllowed)) {
        return third_party_dlls::kUnexpectedLog;
      }

      if (test_id == third_party_dlls::kTestLogPath &&
          !MatchPath(dll_name, *entry))
        return third_party_dlls::kUnexpectedSectionPath;

      return code;
    }

    default:
      return third_party_dlls::kUnsupportedTestId;
  }

  return 0;
}
