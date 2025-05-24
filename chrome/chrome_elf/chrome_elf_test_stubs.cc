// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/win/windows_types.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#include "chrome/common/chrome_switches.h"

// This function is a temporary workaround for https://crbug.com/655788. We
// need to come up with a better way to initialize crash reporting that can
// happen inside DllMain().
void SignalInitializeCrashReporting() {}

void SignalChromeElf() {}

bool GetUserDataDirectoryThunk(wchar_t* user_data_dir,
                               size_t user_data_dir_length,
                               wchar_t* invalid_user_data_dir,
                               size_t invalid_user_data_dir_length) {
  // In tests, just respect the user-data-dir switch if given.
  base::FilePath user_data_dir_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kUserDataDir);
  if (!user_data_dir_path.empty() && user_data_dir_path.EndsWithSeparator())
    user_data_dir_path = user_data_dir_path.StripTrailingSeparators();

  wcsncpy_s(user_data_dir, user_data_dir_length,
            user_data_dir_path.value().c_str(), _TRUNCATE);
  wcsncpy_s(invalid_user_data_dir, invalid_user_data_dir_length, L"",
            _TRUNCATE);

  return !user_data_dir_path.empty();
}

bool IsTemporaryUserDataDirectoryCreatedForHeadless() {
  return false;
}

void SetMetricsClientId(const char* client_id) {}

//------------------------------------------------------------------------------
// chrome\chrome_elf\third_party_dlls export test stubs.
// - For use by \\chrome\browser\conflicts\* testing.
// - Stubs should shadow third_party_dlls\public_api.h and logs_unittest.cc.
//------------------------------------------------------------------------------

bool IsThirdPartyInitialized() {
  return false;
}

struct TestLogEntry {
  third_party_dlls::LogType log_type;
  uint32_t module_size;
  uint32_t time_date_stamp;
};

// This test stub always writes 2 hardcoded entries into the buffer, if the
// buffer size is large enough.
uint32_t DrainLog(uint8_t* buffer,
                  uint32_t buffer_size,
                  uint32_t* log_remaining) {
  // Alternate between log types.
  TestLogEntry kTestLogEntries[] = {
      {third_party_dlls::LogType::kAllowed, 0x9901, 0x12345678},
      {third_party_dlls::LogType::kBlocked, 0x9902, 0x12345678},
  };

  // Each entry shares the module path for convenience.
  static constexpr char kModulePath[] = "C:\\foo\\bar\\module.dll";
  static constexpr uint32_t kModulePathLength = std::size(kModulePath) - 1;

  if (log_remaining) {
    *log_remaining = third_party_dlls::GetLogEntrySize(kModulePathLength) *
                     std::size(kTestLogEntries);
  }

  uint8_t* tracker = buffer;
  for (const auto& test_entry : kTestLogEntries) {
    uint32_t entry_size = third_party_dlls::GetLogEntrySize(kModulePathLength);
    if (tracker + entry_size > buffer + buffer_size)
      break;

    third_party_dlls::LogEntry* log_entry =
        reinterpret_cast<third_party_dlls::LogEntry*>(tracker);

    log_entry->type = test_entry.log_type;
    log_entry->module_size = test_entry.module_size;
    log_entry->time_date_stamp = test_entry.time_date_stamp;
    log_entry->path_len = kModulePathLength;
    ::memcpy(log_entry->path, kModulePath, log_entry->path_len + 1);

    tracker += entry_size;
  }

  return base::checked_cast<uint32_t>(tracker - buffer);
}

bool RegisterLogNotification(HANDLE event_handle) {
  return true;
}

uint32_t GetBlockedModulesCount() {
  return 0;
}

uint32_t GetUniqueBlockedModulesCount() {
  return 0;
}

void DisableHook() {}

int32_t GetApplyHookResult() {
  return 0;
}

bool IsExtensionPointDisableSet() {
  return false;
}
