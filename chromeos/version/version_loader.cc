// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/version/version_loader.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace chromeos {
namespace version_loader {

namespace {

// Beginning of line we look for that gives full version number.
// Format: x.x.xx.x (Developer|Official build extra info) board info
const char kFullVersionKey[] = "CHROMEOS_RELEASE_DESCRIPTION";

// Same but for short version (x.x.xx.x).
const char kVersionKey[] = "CHROMEOS_RELEASE_VERSION";

// Same but for ARC version.
const char kArcVersionKey[] = "CHROMEOS_ARC_VERSION";

// Same but for ARC Android SDK Version
const char kArcAndroidSdkVersionKey[] = "CHROMEOS_ARC_ANDROID_SDK_VERSION";

// Beginning of line we look for that gives the firmware version.
const char kFirmwarePrefix[] = "version";

// File to look for firmware number in.
const char kPathFirmware[] = "/var/log/bios_info.txt";

}  // namespace

std::optional<std::string> GetVersion(VersionFormat format) {
  std::string version;
  std::string key = (format == VERSION_FULL ? kFullVersionKey : kVersionKey);
  if (!base::SysInfo::GetLsbReleaseValue(key, &version)) {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "No LSB version key: " << key;
    return std::nullopt;
  }
  if (format == VERSION_SHORT_WITH_DATE) {
    version += base::UnlocalizedTimeFormatWithPattern(
        base::SysInfo::GetLsbReleaseTime(), "-yy.MM.dd",
        icu::TimeZone::getGMT());
  }

  return version;
}

std::string GetArcVersion() {
  std::string version;
  if (!base::SysInfo::GetLsbReleaseValue(kArcVersionKey, &version)) {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "No LSB version key: " << kArcVersionKey;
  }
  return version;
}

std::optional<std::string> GetArcAndroidSdkVersion() {
  std::string arc_sdk_version;

  if (!base::SysInfo::GetLsbReleaseValue(kArcAndroidSdkVersionKey,
                                         &arc_sdk_version)) {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "No LSB version key: " << kArcAndroidSdkVersionKey;
    return std::nullopt;
  }

  return arc_sdk_version;
}

std::string GetFirmware() {
  std::string firmware;
  std::string contents;
  const base::FilePath file_path(kPathFirmware);
  if (base::ReadFileToString(file_path, &contents)) {
    firmware = ParseFirmware(contents);
  }
  return firmware;
}

std::string ParseFirmware(const std::string& contents) {
  // The file contains lines such as:
  // vendor           | ...
  // version          | ...
  // release_date     | ...
  // We don't make any assumption that the spaces between "version" and "|" is
  //   fixed. So we just match kFirmwarePrefix at the start of the line and find
  //   the first character that is not "|" or space

  std::string_view firmware_prefix(kFirmwarePrefix);
  for (const std::string& line : base::SplitString(
           contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (base::StartsWith(line, firmware_prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      std::string str = line.substr(firmware_prefix.size());
      size_t found = str.find_first_not_of("| ");
      if (found != std::string::npos)
        return str.substr(found);
    }
  }
  return std::string();
}

bool IsRollback(const std::string& current_version,
                const std::string& new_version) {
  VLOG(1) << "Current version: " << current_version;
  VLOG(1) << "New version: " << new_version;

  if (new_version == "0.0.0.0") {
    // No update available.
    return false;
  }

  std::vector<std::string> current_version_parts = base::SplitString(
      current_version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<std::string> new_version_parts = base::SplitString(
      new_version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  for (size_t i = 0;
       i < current_version_parts.size() && i < new_version_parts.size(); ++i) {
    if (current_version_parts[i] == new_version_parts[i])
      continue;

    unsigned int current_part, new_part;
    if (!base::StringToUint(current_version_parts[i], &current_part) ||
        !base::StringToUint(new_version_parts[i], &new_part)) {
      // One of the parts is not a number (e.g. date in test builds), compare
      // strings.
      return current_version_parts[i] > new_version_parts[i];
    }
    return current_part > new_part;
  }

  // Return true if new version is prefix of current version, false otherwise.
  return new_version_parts.size() < current_version_parts.size();
}

}  // namespace version_loader
}  // namespace chromeos
