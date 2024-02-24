// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/caller_validation.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <psapi.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"

namespace elevation_service {

namespace {

constexpr char kPathValidationPrefix[] = "PATH";
constexpr char kNoneValidationPrefix[] = "NONE";

// Paths look like this: "\Device\HarddiskVolume6\Program Files\Blah\app.exe".
// This function will remove the final EXE, then it will remove paths that match
// 'Temp' or 'Application' if they are the final directory.
//
// Examples:
// "\Device\HarddiskVolume6\Program Files\Blah\app.exe" ->
// "\Device\HarddiskVolume6\Program Files\Blah\"
//
// "\Device\HarddiskVolume6\Program Files\Blah\app2.exe" ->
// "\Device\HarddiskVolume6\Program Files\Blah\"
//
// "\Device\HarddiskVolume6\Program Files\Blah\Temp\app.exe" ->
// "\Device\HarddiskVolume6\Program Files\Blah\"
//
// "\Device\HarddiskVolume6\Program Files\Blah\Application\app.exe" ->
// "\Device\HarddiskVolume6\Program Files\Blah\"
//
// Note: base::FilePath is not used here because NT paths are not real paths.
std::string MaybeTrimProcessPath(const std::string& full_path) {
  auto tokens = base::SplitString(full_path, "\\", base::KEEP_WHITESPACE,
                                  base::SPLIT_WANT_ALL);
  std::string output;
  size_t token = 0;
  for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
    token++;
    if (token == 1 &&
        base::EndsWith(*it, ".exe", base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    if (token == 2 && (base::EqualsCaseInsensitiveASCII(*it, "Temp") ||
                       base::EqualsCaseInsensitiveASCII(*it, "Application"))) {
      continue;
    }
    output = *it + "\\" + output;
  }
  return output;
}

std::string GetProcessExecutablePath(const base::Process& process) {
  std::string image_path(MAX_PATH, L'\0');
  DWORD path_length = image_path.size();
  BOOL success = ::QueryFullProcessImageNameA(
      process.Handle(), PROCESS_NAME_NATIVE, image_path.data(), &path_length);
  if (!success && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // Process name is potentially greater than MAX_PATH, try larger max size.
    // https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    image_path.resize(UNICODE_STRING_MAX_CHARS);
    path_length = image_path.size();
    success = ::QueryFullProcessImageNameA(
        process.Handle(), PROCESS_NAME_NATIVE, image_path.data(), &path_length);
  }
  if (!success) {
    PLOG_IF(ERROR, ::GetLastError() != ERROR_GEN_FAILURE)
        << "Failed to get process image path";
    return std::string();
  }
  image_path.resize(path_length);
  return image_path;
}

// Generate path based validation data, or return empty string if this was not
// possible.
base::expected<std::string, HRESULT> GeneratePathValidationData(
    const base::Process& process) {
  auto path = GetProcessExecutablePath(process);
  if (path.empty()) {
    return base::unexpected(
        elevation_service::Elevator::kErrorCouldNotObtainPath);
  }
  // Application identity capture for encrypt is only supported on local paths.
  if (!base::StartsWith(path, "\\Device\\HarddiskVolume",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return base::unexpected(
        elevation_service::Elevator::kErrorUnsupportedFilePath);
  }
  return path;
}

bool ValidatePath(const base::Process& process, const std::string& data) {
  return MaybeTrimProcessPath(data) ==
         MaybeTrimProcessPath(GetProcessExecutablePath(process));
}

}  // namespace

base::expected<std::string, HRESULT> GenerateValidationData(
    ProtectionLevel level,
    const base::Process& process) {
  switch (level) {
    case ProtectionLevel::NONE:
      return kNoneValidationPrefix;
    case ProtectionLevel::PATH_VALIDATION:
      auto path_validation_data = GeneratePathValidationData(process);
      if (path_validation_data.has_value()) {
        path_validation_data->insert(0, kPathValidationPrefix);
      }
      return path_validation_data;
  }
}

bool ValidateData(const base::Process& process,
                  const std::string& validation_data) {
  // Determine which kind of validation was requested.
  if (base::StartsWith(validation_data, kNoneValidationPrefix,
                       base::CompareCase::SENSITIVE)) {
    // No validation always returns true.
    return true;
  } else if (base::StartsWith(validation_data, kPathValidationPrefix,
                              base::CompareCase::SENSITIVE)) {
    // Strip off the path validation header.
    const std::string path_validation_data =
        validation_data.substr(sizeof(kPathValidationPrefix) - 1);
    // Defer to the path validation.
    return ValidatePath(process, path_validation_data);
  }
  return false;
}

}  // namespace elevation_service
