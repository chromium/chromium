// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/caller_validation.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <psapi.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/types/expected.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"

namespace elevation_service {

namespace {

// Paths look like this: "C:\Program Files\Blah\app.exe".
// This function will remove the final EXE, then it will remove paths that match
// 'Temp' or 'Application' if they are the final directory.
//
// Examples:
// "C:\Program Files\Blah\app.exe" ->
// "C:\Program Files\Blah"
//
// "C:\Program Files\Blah\app2.exe" ->
// "C:\Program Files\Blah"
//
// "C:\Program Files\Blah\Temp\app.exe" ->
// "C:\Program Files\Blah"
//
// "C:\Program Files\Blah\Application\app.exe" ->
// "C:\Program Files\Blah"
//
// "C:\Program Files (x86)\Blah\Application\app.exe" ->
// "C:\Program Files\Blah"
//
base::FilePath MaybeTrimProcessPath(const base::FilePath& full_path) {
  auto components = full_path.GetComponents();
  std::vector<std::wstring> trimmed_components;

  size_t token = 0;
  for (auto it = components.crbegin(); it != components.crend(); ++it) {
    token++;
    if (token == 1 &&
        base::EndsWith(*it, L".exe", base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    if (token == 2 && (base::EqualsCaseInsensitiveASCII(*it, "Temp") ||
                       base::EqualsCaseInsensitiveASCII(*it, "Application"))) {
      continue;
    }
    // In Windows Vista and later, the paths to the 'Program Files' and 'Common
    // Files' directories are not localized (translated) on disk. Instead, the
    // localized names are NTFS junction points to the non-localized locations.
    // Since this code is dealing with NT paths the junction has already been
    // resolved to the non-localized version so it is safe to use hard-coded
    // strings here.
    if (base::EqualsCaseInsensitiveASCII(*it, L"Program Files (x86)")) {
      trimmed_components.push_back(L"Program Files");
      continue;
    }
    trimmed_components.push_back(*it);
  }
  base::FilePath trimmed_path;
  for (auto it = trimmed_components.crbegin(); it != trimmed_components.crend();
       ++it) {
    trimmed_path = trimmed_path.Append(*it);
  }
  return trimmed_path;
}

base::expected<base::FilePath, DWORD> GetProcessExecutablePath(
    const base::Process& process) {
  std::wstring image_path(MAX_PATH, L'\0');
  DWORD path_length = image_path.size();
  BOOL success = ::QueryFullProcessImageNameW(process.Handle(), 0,
                                              image_path.data(), &path_length);
  if (!success && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // Process name is potentially greater than MAX_PATH, try larger max size.
    // https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    image_path.resize(UNICODE_STRING_MAX_CHARS);
    path_length = image_path.size();
    success = ::QueryFullProcessImageNameW(process.Handle(), 0,
                                           image_path.data(), &path_length);
  }
  if (!success) {
    PLOG_IF(ERROR, ::GetLastError() != ERROR_GEN_FAILURE)
        << "Failed to get process image path";
    return base::unexpected(::GetLastError());
  }
  return base::FilePath(image_path);
}

// Generate path based validation data, or return empty string if this was not
// possible.
base::expected<std::vector<uint8_t>, HRESULT> GeneratePathValidationData(
    const base::Process& process) {
  auto path = GetProcessExecutablePath(process);
  if (!path.has_value()) {
    return base::unexpected(
        elevation_service::Elevator::kErrorCouldNotObtainPath);
  }
  if (path->IsNetwork()) {
    return base::unexpected(
        elevation_service::Elevator::kErrorUnsupportedFilePath);
  }
  // Wide to narrow data loss is fine here, because the same system will always
  // be dealing with the same data.
  auto narrow_path = base::SysWideToUTF8(MaybeTrimProcessPath(*path).value());
  return std::vector<uint8_t>(narrow_path.begin(), narrow_path.end());
}

bool ValidatePath(const base::Process& process,
                  base::span<const uint8_t> data,
                  std::string* log_message) {
  auto current_path = GeneratePathValidationData(process);
  if (!current_path.has_value()) {
    return false;
  }

  if (data.size() == current_path->size() &&
      std::equal(data.begin(), data.end(), current_path->cbegin())) {
    return true;
  }

  if (log_message) {
    *log_message =
        "Data: '" + std::string(data.begin(), data.end()) + "'. Current: '" +
        std::string(current_path->cbegin(), current_path->cend()) + "'";
  }

  return false;
}

}  // namespace

base::expected<std::vector<uint8_t>, HRESULT> GenerateValidationData(
    ProtectionLevel level,
    const base::Process& process) {
  switch (level) {
    case ProtectionLevel::NONE:
      return std::vector<uint8_t>{ProtectionLevel::NONE};
    case ProtectionLevel::PATH_VALIDATION:
      auto path_validation_data = GeneratePathValidationData(process);
      if (path_validation_data.has_value()) {
        path_validation_data->insert(path_validation_data->cbegin(),
                                     ProtectionLevel::PATH_VALIDATION);
        return *path_validation_data;
      }
      return base::unexpected(path_validation_data.error());
  }
}

bool ValidateData(const base::Process& process,
                  base::span<const uint8_t> validation_data,
                  std::string* log_message) {
  if (validation_data.empty()) {
    return false;
  }

  ProtectionLevel level = static_cast<ProtectionLevel>(validation_data[0]);

  switch (level) {
    case ProtectionLevel::NONE:
      // No validation always returns true.
      return true;
    case ProtectionLevel::PATH_VALIDATION:
      return ValidatePath(process, validation_data.subspan(1), log_message);
  }
}

base::FilePath MaybeTrimProcessPathForTesting(const base::FilePath& full_path) {
  return MaybeTrimProcessPath(full_path);
}

}  // namespace elevation_service
