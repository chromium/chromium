// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_load_attempt_log_listener.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "chrome/browser/win/conflicts/module_blacklist_cache_util.h"
#include "chrome/chrome_elf/third_party_dlls/public_api.h"

namespace {

// Drains the log of blocked modules from chrome_elf.dll.
// Note that the paths returned are device paths, which starts with
// "\Device\Harddisk". They need to be translated to their drive letter path
// equivalent.
std::vector<std::tuple<base::FilePath, uint32_t, uint32_t>>
DrainLogOnBackgroundTask() {
  // Query the number of bytes needed.
  uint32_t bytes_needed = 0;
  DrainLog(nullptr, 0, &bytes_needed);

  // Drain the log.
  auto buffer = std::make_unique<uint8_t[]>(bytes_needed);
  uint32_t bytes_written = DrainLog(buffer.get(), bytes_needed, nullptr);
  DCHECK_EQ(bytes_needed, bytes_written);

  // Parse the data using the recommanded pattern for iterating over the log.
  std::vector<std::tuple<base::FilePath, uint32_t, uint32_t>> blocked_modules;
  uint8_t* tracker = buffer.get();
  uint8_t* buffer_end = buffer.get() + bytes_written;
  while (tracker < buffer_end) {
    third_party_dlls::LogEntry* entry =
        reinterpret_cast<third_party_dlls::LogEntry*>(tracker);
    DCHECK_LE(tracker + third_party_dlls::GetLogEntrySize(entry->path_len),
              buffer_end);

    // Only consider blocked modules.
    // TODO(pmonette): Wire-up loaded modules to ModuleDatabase::OnModuleLoad to
    // get better visibility into all modules that loads into the browser
    // process.
    if (entry->type == third_party_dlls::LogType::kBlocked) {
      // No log path should be empty.
      DCHECK(entry->path_len);
      blocked_modules.emplace_back(
          base::UTF8ToUTF16(base::StringPiece(entry->path, entry->path_len)),
          entry->module_size, entry->time_date_stamp);
    }

    tracker += third_party_dlls::GetLogEntrySize(entry->path_len);
  }

  return blocked_modules;
}

}  // namespace

ModuleLoadAttemptLogListener::ModuleLoadAttemptLogListener(
    OnModuleBlockedCallback on_module_blocked_callback)
    : on_module_blocked_callback_(std::move(on_module_blocked_callback)),
      background_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::MayBlock()})),
      // The event starts signaled so that the logs are drained once when the
      // |object_watcher_| starts waiting on the newly registered event.
      waitable_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::SIGNALED),
      weak_ptr_factory_(this) {
  if (!RegisterLogNotification(waitable_event_.handle()))
    return;

  object_watcher_.StartWatchingMultipleTimes(waitable_event_.handle(), this);
}

ModuleLoadAttemptLogListener::~ModuleLoadAttemptLogListener() = default;

void ModuleLoadAttemptLogListener::OnObjectSignaled(HANDLE object) {
  StartDrainingLogs();
}

void ModuleLoadAttemptLogListener::StartDrainingLogs() {
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&DrainLogOnBackgroundTask),
      base::BindOnce(&ModuleLoadAttemptLogListener::OnLogDrained,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModuleLoadAttemptLogListener::OnLogDrained(
    std::vector<std::tuple<base::FilePath, uint32_t, uint32_t>>&&
        blocked_modules) {
  for (auto& entry : blocked_modules) {
    // Translate the device path to their drive letter equivalent then notify
    // via the callback. The callback is invoked regardless of the result of
    // GetDriveLetterPath() so that the module still shows up in
    // chrome://conflicts.
    base::FilePath module_path = std::move(std::get<0>(entry));
    bool drive_letter_path_found =
        GetDriveLetterPath(module_path, &module_path);
    UMA_HISTOGRAM_BOOLEAN("ThirdPartyModules.GetDriveLetterPathFound",
                          drive_letter_path_found);
    on_module_blocked_callback_.Run(std::move(module_path), std::get<1>(entry),
                                    std::get<2>(entry));
  }
}

bool ModuleLoadAttemptLogListener::GetDriveLetterPath(
    const base::FilePath& device_path,
    base::FilePath* drive_letter_path) {
  for (size_t retry_count = 0; retry_count < 2; ++retry_count) {
    // Only update the mapping if a matching device root wasn't found.
    if (retry_count > 0)
      UpdateDeviceToLetterPathMapping();

    for (const auto& element : device_to_letter_path_mapping_) {
      const base::FilePath& device_root = element.first;
      const base::string16& drive_letter_root = element.second;
      if (device_root.IsParent(device_path)) {
        *drive_letter_path = base::FilePath(
            drive_letter_root +
            device_path.value().substr(device_root.value().length()));
        return true;
      }
    }
  }

  return false;
}

void ModuleLoadAttemptLogListener::UpdateDeviceToLetterPathMapping() {
  const int kDriveMappingSize = 1024;
  wchar_t drive_mapping[kDriveMappingSize] = {'\0'};
  if (!::GetLogicalDriveStrings(kDriveMappingSize - 1, drive_mapping))
    return;

  device_to_letter_path_mapping_.clear();

  wchar_t* drive_map_ptr = drive_mapping;
  wchar_t device_path_as_string[MAX_PATH];
  wchar_t drive[] = L" :";

  while (*drive_map_ptr) {
    drive[0] = drive_map_ptr[0];  // Copy the drive letter.

    if (::QueryDosDevice(drive, device_path_as_string, MAX_PATH)) {
      device_to_letter_path_mapping_.emplace_back(
          base::FilePath(device_path_as_string), drive);
    }

    // Move to the next drive letter string, which starts one
    // increment after the '\0' that terminates the current string.
    while (*drive_map_ptr++) {
    }
  }
}
