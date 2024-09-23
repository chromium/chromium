// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_load_attempt_log_listener.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/aligned_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split_win.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/win/conflicts/module_blocklist_cache_util.h"
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
  auto buffer = base::HeapArray<uint8_t>::WithSize(bytes_needed);
  uint32_t bytes_written = DrainLog(buffer.data(), buffer.size(), nullptr);
  CHECK_EQ(bytes_needed, bytes_written);

  // Parse the data.
  std::vector<std::tuple<base::FilePath, uint32_t, uint32_t>> blocked_modules;

  auto reader = base::SpanReader(buffer.as_span());
  // Need to read at least sizeof(third_party_dlls::LogEntry) bytes to figure
  // out the real size of log entry.
  while (auto span = reader.Read(sizeof(third_party_dlls::LogEntry))) {
    third_party_dlls::LogEntry* entry =
        reinterpret_cast<third_party_dlls::LogEntry*>(span->data());
    CHECK(base::IsAligned(span->data(), alignof(third_party_dlls::LogEntry)));

    // Now get the real size of the log entry and ensure it fully fits inside
    // the buffer.
    uint32_t log_entry_size =
        third_party_dlls::GetLogEntrySize(entry->path_len);
    CHECK(reader.Skip(log_entry_size - sizeof(third_party_dlls::LogEntry)));

    // Only consider blocked modules.
    // TODO(pmonette): Wire-up loaded modules to ModuleDatabase::OnModuleLoad to
    // get better visibility into all modules that loads into the browser
    // process.
    if (entry->type == third_party_dlls::LogType::kBlocked) {
      // No log path should be empty.
      CHECK(entry->path_len);
      blocked_modules.emplace_back(
          base::UTF8ToWide(std::string_view(entry->path, entry->path_len)),
          entry->module_size, entry->time_date_stamp);
    }
  }

  return blocked_modules;
}

}  // namespace

ModuleLoadAttemptLogListener::ModuleLoadAttemptLogListener(
    OnModuleBlockedCallback on_module_blocked_callback)
    : on_module_blocked_callback_(std::move(on_module_blocked_callback)),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
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
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DrainLogOnBackgroundTask),
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
      const std::wstring& drive_letter_root = element.second;
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
  // Note: There are 26 letters possible, and each entry takes 4 characters of
  // space (e.g. ['C', ':', '\\', '\0'] plus an additional NUL character at the
  // end, meaning 128 is safely above the maximum possible size needed).
  std::array<wchar_t, 128> drive_strings_buffer = {};
  DWORD count = ::GetLogicalDriveStrings(drive_strings_buffer.size() - 1u,
                                         drive_strings_buffer.data());
  CHECK_LT(count, drive_strings_buffer.size());
  if (!count) {
    // GetLogicalDriveStrings failed.
    return;
  }
  // Truncate the buffer to the bytes actually copied by GetLogicalDriveStrings.
  // Note: This gets rid of the superfluous NUL character at the end. Thus,
  // `drive_strings` is now a sequence of null terminated strings.
  std::wstring_view drive_strings(drive_strings_buffer.data(), count);

  device_to_letter_path_mapping_.clear();
  for (std::wstring_view drive_string :
       SplitLogicalDriveStringsImpl(drive_strings)) {
    wchar_t drive[] = L" :";
    drive[0] = drive_string[0];  // Copy the drive letter.

    wchar_t device_path[MAX_PATH];
    if (::QueryDosDevice(drive, device_path, std::size(device_path))) {
      device_to_letter_path_mapping_.emplace_back(base::FilePath(device_path),
                                                  drive);
    }
  }
}

// static
std::vector<std::wstring_view>
ModuleLoadAttemptLogListener::SplitLogicalDriveStringsImpl(
    std::wstring_view logical_drive_strings) {
  return base::SplitStringPiece(
      logical_drive_strings, base::MakeStringViewWithNulChars(L"\0"),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}
