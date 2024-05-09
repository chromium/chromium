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
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"

namespace elevation_service {

namespace {

// Paths look like this: "C:\Program Files\Blah\app.exe".
// This function will remove the final EXE, then it will remove paths that match
// 'Temp', 'Application' or a version pattern if they are the final directory.
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
// "C:\Program Files (x86)\Blah\Application\1.2.3.4\app.exe" ->
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
    if (token == 2 && it->starts_with(L"scoped_dir")) {
      token--;
      continue;
    }
    if (token == 2 && base::Version(base::WideToASCII(it->data())).IsValid()) {
      token--;
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
    const base::FilePath& process_path) {
  if (process_path.IsNetwork()) {
    return base::unexpected(
        elevation_service::Elevator::kErrorUnsupportedFilePath);
  }
  // Wide to narrow data loss is fine here, because the same system will always
  // be dealing with the same data.
  const auto narrow_path =
      base::SysWideToUTF8(MaybeTrimProcessPath(process_path).value());
  return std::vector<uint8_t>(narrow_path.begin(), narrow_path.end());
}

HRESULT ValidatePath(const base::Process& process,
                     base::span<const uint8_t> data,
                     std::string* log_message) {
  const auto process_path = GetProcessExecutablePath(process);
  if (!process_path.has_value()) {
    return elevation_service::Elevator::kErrorCouldNotObtainPath;
  }

  auto current_path_data = GeneratePathValidationData(*process_path);
  if (!current_path_data.has_value()) {
    return current_path_data.error();
  }

  if (data.size() == current_path_data->size() &&
      std::equal(data.begin(), data.end(), current_path_data->cbegin())) {
    return S_OK;
  }

  SYSLOG(WARNING) << "Failed to authenticate caller process: "
                  << process_path->value();

  if (log_message) {
    *log_message =
        "Data: '" + std::string(data.begin(), data.end()) + "'. Current: '" +
        std::string(current_path_data->cbegin(), current_path_data->cend()) +
        "'";
  }

  return elevation_service::Elevator::kValidationDidNotPass;
}

}  // namespace

base::expected<std::vector<uint8_t>, HRESULT> GenerateValidationData(
    ProtectionLevel level,
    const base::Process& process) {
  switch (level) {
    case ProtectionLevel::PROTECTION_NONE:
      return std::vector<uint8_t>{ProtectionLevel::PROTECTION_NONE};
    case ProtectionLevel::PROTECTION_PATH_VALIDATION_OLD:
      return base::unexpected(
          elevation_service::Elevator::kErrorUnsupportedProtectionLevel);
    case ProtectionLevel::PROTECTION_PATH_VALIDATION: {
      const auto process_path = GetProcessExecutablePath(process);
      if (!process_path.has_value()) {
        return base::unexpected(
            elevation_service::Elevator::kErrorCouldNotObtainPath);
      }
      auto path_validation_data = GeneratePathValidationData(*process_path);
      if (path_validation_data.has_value()) {
        path_validation_data->insert(
            path_validation_data->cbegin(),
            ProtectionLevel::PROTECTION_PATH_VALIDATION);
        return *path_validation_data;
      }
      return base::unexpected(path_validation_data.error());
    }
    case ProtectionLevel::PROTECTION_MAX:
      return base::unexpected(
          elevation_service::Elevator::kErrorUnsupportedProtectionLevel);
  }
}

HRESULT ValidateData(const base::Process& process,
                     base::span<const uint8_t> validation_data,
                     std::string* log_message) {
  if (validation_data.empty()) {
    return E_INVALIDARG;
  }

  ProtectionLevel level = static_cast<ProtectionLevel>(validation_data[0]);

  if (level >= ProtectionLevel::PROTECTION_MAX) {
    return E_INVALIDARG;
  }

  switch (level) {
    case ProtectionLevel::PROTECTION_NONE:
      // No validation always returns true.
      return S_OK;
    case ProtectionLevel::PROTECTION_PATH_VALIDATION_OLD:
    case ProtectionLevel::PROTECTION_PATH_VALIDATION:
      return ValidatePath(process, validation_data.subspan(1), log_message);
    case ProtectionLevel::PROTECTION_MAX:
      return E_INVALIDARG;
  }
}

base::FilePath MaybeTrimProcessPathForTesting(const base::FilePath& full_path) {
  return MaybeTrimProcessPath(full_path);
}

}  // namespace elevation_service
