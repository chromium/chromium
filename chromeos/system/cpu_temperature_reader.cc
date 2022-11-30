// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/cpu_temperature_reader.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"

namespace chromeos {
namespace system {

using CPUTemperatureInfo = CPUTemperatureReader::CPUTemperatureInfo;

namespace {

// The location we read our CPU temperature and channel label from.
constexpr char kDefaultHwmonDir[] = "/sys/class/hwmon/";
constexpr char kDeviceDir[] = "device";
constexpr char kHwmonDirectoryPattern[] = "hwmon*";
constexpr char kCPUTempFilePattern[] = "temp*_input";

// The contents of sysfs files might contain a newline at the end. Use this
// function to read from a sysfs file and remove the newline.
bool ReadFileContentsAndTrimWhitespace(const base::FilePath& path,
                                       std::string* contents_out) {
  if (!base::ReadFileToString(path, contents_out))
    return false;
  base::TrimWhitespaceASCII(*contents_out, base::TRIM_TRAILING, contents_out);
  return true;
}

// Reads the temperature as degrees Celsius from the file at |path|, which is
// expected to contain the temperature in millidegrees Celsius. Converts the
// value into degrees and then stores it in |*temp_celsius_out|. returns true if
// the value was successfully read from file, parsed as an int, and converted.
bool ReadTemperatureFromPath(const base::FilePath& path,
                             double* temp_celsius_out) {
  std::string temperature_string;
  if (!ReadFileContentsAndTrimWhitespace(path, &temperature_string))
    return false;
  uint32_t temperature = 0;
  if (!base::StringToUint(temperature_string, &temperature))
    return false;
  *temp_celsius_out = temperature / 1000.0;
  return true;
}

// Gets the label describing this temperature. Use the file "temp*_label" if it
// is present, or fall back on the file "name" or |label_path|.
std::string GetLabelFromPath(const base::FilePath& label_path) {
  std::string label;
  if (base::PathExists(label_path) &&
      ReadFileContentsAndTrimWhitespace(base::FilePath(label_path), &label) &&
      !label.empty()) {
    return label;
  }

  base::FilePath name_path = label_path.DirName().Append("name");
  if (base::PathExists(name_path) &&
      ReadFileContentsAndTrimWhitespace(name_path, &label) && !label.empty()) {
    return label;
  }
  return label_path.value();
}

void ReadTemperaturesFromDirectory(const base::FilePath& hwmon_path,
                                   std::vector<CPUTemperatureInfo>* result) {
  base::FileEnumerator enumerator(
      hwmon_path, false, base::FileEnumerator::FILES, kCPUTempFilePattern);
  for (base::FilePath temperature_path = enumerator.Next();
       !temperature_path.empty(); temperature_path = enumerator.Next()) {
    CPUTemperatureInfo info;
    if (!ReadTemperatureFromPath(temperature_path, &info.temp_celsius)) {
      DLOG(WARNING) << "Unable to read CPU temperature from "
                    << temperature_path.value();
      continue;
    }
    // Get appropriate temp*_label file.
    std::string label_path = temperature_path.value();
    base::ReplaceSubstringsAfterOffset(&label_path, 0, "input", "label");
    info.label = GetLabelFromPath(base::FilePath(label_path));
    result->push_back(info);
  }
}

}  // namespace

CPUTemperatureReader::CPUTemperatureInfo::CPUTemperatureInfo() = default;

CPUTemperatureReader::CPUTemperatureInfo::~CPUTemperatureInfo() = default;

CPUTemperatureReader::CPUTemperatureReader() : hwmon_dir_(kDefaultHwmonDir) {}

CPUTemperatureReader::~CPUTemperatureReader() = default;

std::vector<CPUTemperatureInfo> CPUTemperatureReader::GetCPUTemperatures() {
  std::vector<CPUTemperatureInfo> result;

  // Get directories /sys/class/hwmon/hwmon*.
  base::FileEnumerator hwmon_enumerator(hwmon_dir_, false,
                                        base::FileEnumerator::DIRECTORIES,
                                        kHwmonDirectoryPattern);
  for (base::FilePath hwmon_path = hwmon_enumerator.Next(); !hwmon_path.empty();
       hwmon_path = hwmon_enumerator.Next()) {
    // Get temp*_input files in hwmon*/ and hwmon*/device/. A survey of DUTs
    // suggests that temp values are contained in hwmon*/device on kernel 3.14
    // and earlier and directly in hwmon*/ in kernel 3.18 and later. When no
    // device subdirectory is present, the temp values are always contained
    // directly within the hwmon directory.
    ReadTemperaturesFromDirectory(hwmon_path, &result);
    if (base::PathExists(hwmon_path.Append(kDeviceDir))) {
      ReadTemperaturesFromDirectory(hwmon_path.Append(kDeviceDir), &result);
    }
  }
  return result;
}

}  // namespace system
}  // namespace chromeos
