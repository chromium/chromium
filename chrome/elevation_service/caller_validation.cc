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
#include "base/strings/string_util.h"
#include "chrome/elevation_service/elevation_service_idl.h"

namespace elevation_service {

namespace {

constexpr char kPathValidationPrefix[] = "PATH";
constexpr char kNoneValidationPrefix[] = "NONE";

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
std::string GeneratePathValidationData(const base::Process& process) {
  return GetProcessExecutablePath(process);
}

bool ValidatePath(const base::Process& process, const std::string& data) {
  return data == GetProcessExecutablePath(process);
}

}  // namespace

std::string GenerateValidationData(ProtectionLevel level,
                                   const base::Process& process) {
  std::string validation_data;
  switch (level) {
    case ProtectionLevel::NONE:
      validation_data.insert(0, kNoneValidationPrefix);
      break;
    case ProtectionLevel::PATH_VALIDATION:
      validation_data = GeneratePathValidationData(process);
      if (validation_data.empty())
        return std::string();
      validation_data.insert(0, kPathValidationPrefix);
      break;
  }
  return validation_data;
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
