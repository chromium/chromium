// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/registry_logger.h"

#include <stdint.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

namespace {

const wchar_t kMultiSzSeparator = L'\0';  // Must be null char.

// The separator using to join and split pending log files.
const wchar_t* kPendingLogFilesSeparatorForLogs = L":";

// Maximum length for a registry name, according to Microsoft documentation.
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724872(v=vs.85).aspx
static const size_t kMaxRegistryLength = 0x3FFF;

void CreateRegKey(base::win::RegKey* reg_key, const std::wstring& path) {
  if (reg_key->Create(HKEY_CURRENT_USER, path.c_str(),
                      KEY_SET_VALUE | KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to open registry key" << path;
  }
}

}  // namespace

// static
const wchar_t RegistryLogger::kPendingLogFilesValue[] = L"PendingLogs";

// This is an arbitrary truncation length for log upload results, that allows
// for 64 states encoded as 1;0;1;1;. When the series exceeds this length, it is
// truncated to meet it.
// static
const size_t RegistryLogger::kMaxUploadResultLength = 128;

RegistryLogger::RegistryLogger(Mode mode) : RegistryLogger(mode, "") {}

RegistryLogger::RegistryLogger(Mode mode, const std::string& suffix)
    : mode_(mode) {
  if (mode == Mode::NOOP_FOR_TESTING)
    return;

  if (suffix.length() > kMaxRegistryLength) {
    LOG(WARNING) << "Attempted creating registry key longer than limit ("
                 << suffix << "). Logger not initialized.";
    return;
  }

  if (!base::IsStringASCII(suffix)) {
    LOG(WARNING) << "Value of registry suffix (" << suffix
                 << ") must be ASCII. "
                 << "Logger not initialized";
    return;
  }

  suffix_ = base::UTF8ToWide(suffix);
  CreateRegKey(&logging_key_, GetLoggingKeyPath(mode));
  CreateRegKey(&scan_times_key_, GetScanTimesKeyPath(mode));
}

RegistryLogger::~RegistryLogger() {}

void RegistryLogger::WriteExitCode(int exit_code) {
  if (logging_key_.Valid())
    logging_key_.WriteValue(kExitCodeValueName, exit_code);
}

void RegistryLogger::ClearExitCode() {
  if (logging_key_.Valid())
    logging_key_.DeleteValue(kExitCodeValueName);
}

void RegistryLogger::WriteStartTime() {
  if (logging_key_.Valid()) {
    int64_t now = base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
    logging_key_.WriteValue(kStartTimeValueName, &now, sizeof(now), REG_QWORD);
  }
}

void RegistryLogger::WriteEndTime() {
  if (logging_key_.Valid()) {
    int64_t now = base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
    logging_key_.WriteValue(kEndTimeValueName, &now, sizeof(now), REG_QWORD);
  }
}

void RegistryLogger::ClearEndTime() {
  if (logging_key_.Valid())
    logging_key_.DeleteValue(kEndTimeValueName);
}

void RegistryLogger::ClearScanTimes() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!scan_times_key_.Valid())
    return;

  if (scan_times_key_.DeleteKey(L"") != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to delete key: '" << GetScanTimesKeyPath(mode_)
                << "'";
    return;
  }

  CreateRegKey(&scan_times_key_, GetScanTimesKeyPath(mode_));
}

void RegistryLogger::WriteMemoryUsage(size_t memory_used_kb) {
  if (logging_key_.Valid()) {
    DWORD memory = memory_used_kb;
    LONG result = logging_key_.WriteValue(kMemoryUsedValueName, memory);
    PLOG_IF(ERROR, result != ERROR_SUCCESS)
        << "Failed to write memory usage to the registry. Error: " << std::hex
        << result;
  }
}

void RegistryLogger::AppendLogUploadResult(bool success) {
  if (logging_key_.Valid()) {
    std::wstring upload_results;
    // Ignore the return value, if this fails, just overwrite what is there.
    LONG result =
        logging_key_.ReadValue(kUploadResultsValueName, &upload_results);
    PLOG_IF(ERROR, result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
        << "Failed to read log upload results.";

    upload_results += success ? L"1;" : L"0;";
    if (upload_results.size() > kMaxUploadResultLength) {
      upload_results.erase(upload_results.begin(),
                           upload_results.begin() + (upload_results.size() -
                                                     kMaxUploadResultLength));
    }

    result = logging_key_.WriteValue(kUploadResultsValueName,
                                     upload_results.c_str());
    PLOG_IF(ERROR, result != ERROR_SUCCESS)
        << "Failed to persist log upload results.";
  }
}

void RegistryLogger::WriteReporterLogsUploadResult(
    SafeBrowsingReporter::Result upload_result) {
  DCHECK(mode_ == Mode::REPORTER);

  if (logging_key_.Valid()) {
    LONG result = logging_key_.WriteValue(kLogsUploadResultValueName,
                                          static_cast<DWORD>(upload_result));
    PLOG_IF(ERROR, result != ERROR_SUCCESS)
        << "Failed to persist log upload results.";
  }
}

bool RegistryLogger::AppendLogFilePath(const base::FilePath& log_file) {
  if (!logging_key_.Valid())
    return false;

  std::wstring registry_value;
  std::vector<std::wstring> log_files;
  if (ReadPendingLogFiles(&log_files, nullptr)) {
    log_files.push_back(log_file.value());
    registry_value =
        base::JoinString(log_files, base::WStringPiece(&kMultiSzSeparator, 1));
  } else {
    registry_value = log_file.value();
  }

  // REG_MULTI_SZ requires an extra \0 at the end of the string.
  registry_value.append(1, L'\0');
  LONG result = logging_key_.WriteValue(
      kPendingLogFilesValue,
      reinterpret_cast<const void*>(registry_value.c_str()),
      registry_value.size() * sizeof(std::wstring::value_type), REG_MULTI_SZ);
  if (result != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to write '" << registry_value
                << "' to pending logs registry entry. Error: " << result;
    return false;
  }
  return true;
}

// TODO(csharp): Maybe optimize these, which run in O(n^2) when used
// subsequently to remove all pending log files for example.
void RegistryLogger::GetNextLogFilePath(base::FilePath* log_file) {
  DCHECK(log_file);
  log_file->clear();
  if (!logging_key_.Valid() || !logging_key_.HasValue(kPendingLogFilesValue))
    return;

  std::vector<std::wstring> log_files;
  RegistryError registry_error = RegistryError::SUCCESS;
  if (!ReadPendingLogFiles(&log_files, &registry_error)) {
    PLOG(WARNING)
        << "Failed to read the list of pending log files (registry_error = "
        << static_cast<int>(registry_error) << ")";
    return;
  }

  if (log_files.empty())
    return;

  *log_file = base::FilePath(log_files[0]);
}

bool RegistryLogger::RemoveLogFilePath(const base::FilePath& log_file) {
  if (!logging_key_.Valid())
    return false;  // Assume empty when we can't read the content.

  if (!logging_key_.HasValue(kPendingLogFilesValue))
    return false;

  std::vector<std::wstring> log_files;
  RegistryError registry_error = RegistryError::SUCCESS;
  if (!ReadPendingLogFiles(&log_files, &registry_error)) {
    PLOG(WARNING) << "Empty pending log files registry entry when trying to "
                  << "remove '" << log_file.value()
                  << "' (registry_error = " << static_cast<int>(registry_error)
                  << ").";
    return false;
  }

  std::vector<std::wstring>::const_iterator iter =
      base::ranges::find(log_files, log_file.value());
  if (iter == log_files.end()) {
    PLOG(WARNING) << "Requested log file '" << SanitizePath(log_file)
                  << "', not found in registered log files '"
                  << base::JoinString(log_files,
                                      kPendingLogFilesSeparatorForLogs)
                  << "'.";
  } else {
    iter = log_files.erase(iter);
  }
  if (log_files.empty()) {
    logging_key_.DeleteValue(kPendingLogFilesValue);
    return false;
  }

  std::wstring registry_value(
      base::JoinString(log_files, base::WStringPiece(&kMultiSzSeparator, 1)));
  // REG_MULTI_SZ requires an extra \0 at the end of the string.
  registry_value.append(1, L'\0');
  LONG result = logging_key_.WriteValue(
      kPendingLogFilesValue,
      reinterpret_cast<const void*>(registry_value.c_str()),
      registry_value.size() * sizeof(std::wstring::value_type), REG_MULTI_SZ);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to write '" << registry_value
               << "' to pending logs registry entry. Error: " << std::hex
               << result;
    // If we fail to write to this key, might as well delete it and let the
    // caller know that there are no more log files.
    logging_key_.DeleteValue(kPendingLogFilesValue);
    return false;
  }

  return true;
}

bool RegistryLogger::RecordFoundPUPs(const std::vector<UwSId>& pups_to_store) {
  std::wstring multi_sz_value;
  for (UwSId pup_to_store : pups_to_store) {
    multi_sz_value += base::NumberToWString(pup_to_store);
    multi_sz_value += kMultiSzSeparator;
  }
  multi_sz_value += kMultiSzSeparator;

  LONG result = logging_key_.WriteValue(
      kFoundUwsValueName, reinterpret_cast<const void*>(multi_sz_value.c_str()),
      multi_sz_value.size() * sizeof(std::wstring::value_type), REG_MULTI_SZ);

  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to write '" << multi_sz_value
               << "' to found UwS registry entry. Error: " << std::hex
               << result;
    return false;
  }

  return true;
}

void RegistryLogger::WriteExperimentalEngineResultCode(int exit_code) {
  if (logging_key_.Valid()) {
    LONG result = logging_key_.WriteValue(kEngineErrorCodeValueName,
                                          static_cast<DWORD>(exit_code));
    PLOG_IF(ERROR, result != ERROR_SUCCESS)
        << "Failed to persist experimental engine error code.";
  }
}

void RegistryLogger::RecordCompletedCleanup() {
  if (logging_key_.Valid()) {
    LONG result = logging_key_.WriteValue(kCleanupCompletedValueName, 1);
    PLOG_IF(ERROR, result != ERROR_SUCCESS)
        << "Failed to persist completed cleanup.";
  }
}

void RegistryLogger::ResetCompletedCleanup() {
  if (logging_key_.Valid() &&
      logging_key_.HasValue(kCleanupCompletedValueName)) {
    LONG result = logging_key_.DeleteValue(kCleanupCompletedValueName);
    PLOG_IF(ERROR, result != ERROR_SUCCESS)
        << "Failed to reset completed cleanup.";
  }
}

std::wstring RegistryLogger::GetLoggingKeyPath(Mode mode) const {
  std::wstring key_path = std::wstring(kSoftwareRemovalToolRegistryKey);
  if (mode == Mode::REMOVER)
    key_path += std::wstring(L"\\") + kCleanerSubKey;
  if (!suffix_.empty())
    key_path += std::wstring(L"\\") + suffix_;
  return key_path;
}

std::wstring RegistryLogger::GetScanTimesKeyPath(Mode mode) const {
  return base::StrCat({GetLoggingKeyPath(mode), L"\\", kScanTimesSubKey});
}

std::wstring RegistryLogger::GetKeySuffix() const {
  return suffix_;
}

// static.
bool RegistryLogger::ReadValues(const base::win::RegKey& logging_key,
                                const wchar_t* name,
                                std::vector<std::wstring>* values,
                                RegistryError* registry_error) {
  DCHECK(name);
  DCHECK(values);
  values->clear();

  std::wstring content;
  uint32_t content_type;
  if (!ReadRegistryValue(logging_key, name, &content, &content_type,
                         registry_error) ||
      content_type != REG_MULTI_SZ) {
    return false;
  }

  // Parse the double-null-terminated list of strings.
  // Note: This code is paranoid to not read outside of |buf|, in the case where
  // it may not be properly terminated.
  if (!content.empty()) {
    const wchar_t* entry = &content[0];
    const wchar_t* buffer_end = entry + content.size();
    while (entry < buffer_end && entry[0] != '\0') {
      const wchar_t* entry_end = std::find(entry, buffer_end, L'\0');
      std::wstring value(entry, entry_end);
      DCHECK(!value.empty());
      values->push_back(value);
      entry = entry_end + 1;
    }
  }
  return true;
}

bool RegistryLogger::ReadPendingLogFiles(std::vector<std::wstring>* log_files,
                                         RegistryError* registry_error) {
  DCHECK(log_files);
  if (!ReadValues(logging_key_, kPendingLogFilesValue, log_files,
                  registry_error)) {
    // Don't log here since in some case this is an expected behavior. Those not
    // expecting a failure are responsible to log appropriately.
    // Delete the value in case it is corrupted, so we can properly use it next
    // time around.
    logging_key_.DeleteValue(kPendingLogFilesValue);
    return false;
  }
  return true;
}

}  // namespace chrome_cleaner
