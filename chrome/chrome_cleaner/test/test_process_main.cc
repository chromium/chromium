// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/initializer.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/secure_dll_loading.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"

namespace {

constexpr base::char16 kLogFileExtension[] = L"log";

}  // namespace

int main(int, char**) {
  // This must be executed as soon as possible to reduce the number of dlls that
  // the code might try to load before we can lock things down.
  chrome_cleaner::EnableSecureDllLoading();

  base::AtExitManager at_exit;

  bool success = base::CommandLine::Init(0, nullptr);
  DCHECK(success);

  if (!chrome_cleaner::SetupTestConfigs())
    return 1;

  // Initialize the logging settings to set a specific log file name.
  base::FilePath exe_file_path =
      chrome_cleaner::PreFetchedPaths::GetInstance()->GetExecutablePath();

  base::FilePath log_file_path(
      exe_file_path.ReplaceExtension(kLogFileExtension));
  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_FILE;
  logging_settings.log_file_path = log_file_path.value().c_str();
  success = logging::InitLogging(logging_settings);
  DCHECK(success);

  LOG(INFO) << "Process Started.";

  chrome_cleaner::NotifyInitializationDoneForTesting();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          chrome_cleaner::kTestForceOverwriteZoneIdentifier)) {
    LOG(INFO) << "Process is overwriting the zone identifier.";
    chrome_cleaner::OverwriteZoneIdentifier(exe_file_path);
  }

  if (command_line->HasSwitch(chrome_cleaner::kTestSleepMinutesSwitch)) {
    std::string value = command_line->GetSwitchValueASCII(
        chrome_cleaner::kTestSleepMinutesSwitch);
    int sleep_minutes = 0;
    if (base::StringToInt(value, &sleep_minutes) && sleep_minutes > 0) {
      LOG(INFO) << "Process is sleeping for " << sleep_minutes << " minutes";
      ::Sleep(base::TimeDelta::FromMinutes(sleep_minutes).InMilliseconds());
    } else {
      LOG(ERROR) << "Invalid sleep delay value " << value;
    }
    NOTREACHED();
    return 1;
  }

  if (command_line->HasSwitch(chrome_cleaner::kTestEventToSignal)) {
    LOG(INFO) << "Process is signaling event '"
              << chrome_cleaner::kTestEventToSignal << "'";
    base::string16 event_name =
        command_line->GetSwitchValueNative(chrome_cleaner::kTestEventToSignal);
    base::win::ScopedHandle handle(
        ::OpenEvent(EVENT_ALL_ACCESS, TRUE, event_name.c_str()));
    PLOG_IF(ERROR, !handle.IsValid())
        << "Cannot create event '" << chrome_cleaner::kTestEventToSignal << "'";
    base::WaitableEvent event(std::move(handle));
    event.Signal();
  }

  // TODO(pmbureau): Add more behavior to test processes termination.

  LOG(INFO) << "Process ended.";
  return 0;
}
