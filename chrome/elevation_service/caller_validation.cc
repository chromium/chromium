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
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "base/win/access_token.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/installer/util/isolation_support.h"

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
    if (token == 2 &&
        it->starts_with(base::ScopedTempDir::GetDefaultTempDirPrefix())) {
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
                     base::span<const uint8_t> data) {
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

  return elevation_service::Elevator::kValidationDidNotPass;
}

bool IsProcessIsolated(const base::Process& process) {
  auto process_token = base::win::AccessToken::FromProcess(process.Handle());

  if (!process_token) {
    return false;
  }

  auto sa = process_token->GetSecurityAttribute(
      installer::GetIsolationAttributeName());
  // The value varies by channel, but existence of the SA means the current
  // process is isolated.
  if (sa.has_value()) {
    return true;
  }
  return false;
}

}  // namespace

base::expected<std::vector<uint8_t>, HRESULT> GenerateValidationData(
    ProtectionLevel level,
    const base::Process& process) {
  const auto process_path = GetProcessExecutablePath(process);

  switch (level) {
    case PROTECTION_NONE:
      return std::vector<uint8_t>{PROTECTION_NONE};
    case PROTECTION_PATH_VALIDATION_OLD:
      return base::unexpected(
          elevation_service::Elevator::kErrorUnsupportedProtectionLevel);
    case PROTECTION_PATH_VALIDATION: {
      if (!process_path.has_value()) {
        return base::unexpected(
            elevation_service::Elevator::kErrorCouldNotObtainPath);
      }

      ASSIGN_OR_RETURN(auto path_validation_data,
                       GeneratePathValidationData(*process_path));
      path_validation_data.insert(path_validation_data.cbegin(),
                                  ProtectionLevel::PROTECTION_PATH_VALIDATION);
      return path_validation_data;
    }
    case PROTECTION_PATH_VALIDATION_WITH_ISOLATION: {
      if (!process_path.has_value()) {
        return base::unexpected(
            elevation_service::Elevator::kErrorCouldNotObtainPath);
      }

      ASSIGN_OR_RETURN(auto validation_data,
                       GeneratePathValidationData(*process_path));
      const auto isolated = IsProcessIsolated(process);

      validation_data.insert(
          validation_data.cbegin(),
          {PROTECTION_PATH_VALIDATION_WITH_ISOLATION, isolated});

      return validation_data;
    }
    case PROTECTION_MAX:
      return base::unexpected(
          elevation_service::Elevator::kErrorUnsupportedProtectionLevel);
  }
}

HRESULT ValidateData(const base::Process& process,
                     base::span<const uint8_t> validation_data) {
  if (validation_data.empty()) {
    return E_INVALIDARG;
  }

  ProtectionLevel level = static_cast<ProtectionLevel>(validation_data[0]);

  if (level >= PROTECTION_MAX) {
    return E_INVALIDARG;
  }

  switch (level) {
    case PROTECTION_NONE:
      // No validation always returns true.
      return S_OK;
    case PROTECTION_PATH_VALIDATION_OLD:
    case PROTECTION_PATH_VALIDATION: {
      const HRESULT path_validation_state =
          ValidatePath(process, validation_data.subspan<1>());
      if (FAILED(path_validation_state)) {
        return path_validation_state;
      }
      // If basic path validation is being used, but the process is isolated,
      // hint to the caller that they should upgrade to
      // PROTECTION_PATH_VALIDATION_WITH_ISOLATION, as it's more secure.
      return IsProcessIsolated(process)
                 ? elevation_service::Elevator::kSuccessShouldReencrypt
                 : path_validation_state;
    }
    case PROTECTION_PATH_VALIDATION_WITH_ISOLATION: {
      if (validation_data.size() < 2) {
        return E_INVALIDARG;
      }
      // Format is {ProtectionLevel, isolation state, path validation data}.
      const HRESULT path_validation_state =
          ValidatePath(process, validation_data.subspan<2>());
      if (FAILED(path_validation_state)) {
        return path_validation_state;
      }
      const bool was_isolated = validation_data[1];
      const bool is_isolated = IsProcessIsolated(process);
      if (was_isolated && !is_isolated) {
        return elevation_service::Elevator::kIsolationStateInvalid;
      }
      // If the process was not isolated before, and it's now isolated, then the
      // data should be re-encrypted to bind it to the isolated state.
      if (is_isolated && !was_isolated) {
        return elevation_service::Elevator::kSuccessShouldReencrypt;
      }
      return path_validation_state;
    }
    case PROTECTION_MAX:
      return E_INVALIDARG;
  }
}

base::FilePath MaybeTrimProcessPathForTesting(const base::FilePath& full_path) {
  return MaybeTrimProcessPath(full_path);
}

}  // namespace elevation_service
