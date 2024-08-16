// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "media/base/media_switches.h"
#endif

namespace {

// Fields used inside the hint file.
const char kPath[] = "Path";
const char kLastBundledVersion[] = "LastBundledVersion";

// Returns the hint file contents as a Value::Dict. Returned result may be an
// empty dictionary if the hint file does not exist or is formatted incorrectly.
base::Value::Dict GetHintFileContents() {
  base::FilePath hint_file_path;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // ChromeOS Lacros should use the Widevine CDM Component Updated by ChromeOS
  // Ash. This is determined by using command line arguments passed when Ash
  // launches Lacros.
  if (!base::FeatureList::IsEnabled(media::kLacrosUseAshWidevine)) {
    DVLOG(1) << "CDM hint file disabled.";
    return base::Value::Dict();
  }

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          switches::kCrosWidevineComponentUpdatedHintFile)) {
    DVLOG(1) << "Command line switch " +
                    std::string(
                        switches::kCrosWidevineComponentUpdatedHintFile) +
                    " not found.";
    return base::Value::Dict();
  }

  hint_file_path = command_line->GetSwitchValuePath(
      switches::kCrosWidevineComponentUpdatedHintFile);
#else
  CHECK(base::PathService::Get(chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT,
                               &hint_file_path));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  DVLOG(1) << __func__ << " checking " << hint_file_path;
  if (!base::PathExists(hint_file_path)) {
    DVLOG(1) << "CDM hint file at " << hint_file_path << " does not exist.";
    return base::Value::Dict();
  }

  std::string json_string;
  if (!base::ReadFileToString(hint_file_path, &json_string)) {
    DLOG(ERROR) << "Could not read the CDM hint file at " << hint_file_path;
    return base::Value::Dict();
  }

  std::string error_message;
  JSONStringValueDeserializer deserializer(json_string);
  std::unique_ptr<base::Value> dict =
      deserializer.Deserialize(/*error_code=*/nullptr, &error_message);

  if (!dict || !dict->is_dict()) {
    DLOG(ERROR) << "Could not deserialize the CDM hint file. Error: "
                << error_message;
    return base::Value::Dict();
  }

  return std::move(*dict).TakeDict();
}

}  // namespace

bool UpdateWidevineCdmHintFile(const base::FilePath& cdm_base_path,
                               std::optional<base::Version> bundled_version) {
  DCHECK(!cdm_base_path.empty());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  NOTREACHED() << "Lacros should not be updating the hint file.";
#else
  base::FilePath hint_file_path;
  CHECK(base::PathService::Get(chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT,
                               &hint_file_path));

  base::Value::Dict dict;
  dict.Set(kPath, cdm_base_path.value());
  if (bundled_version.has_value()) {
    dict.Set(kLastBundledVersion, bundled_version.value().GetString());
  }

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  if (!serializer.Serialize(dict)) {
    DLOG(ERROR) << "Could not serialize the CDM hint file.";
    return false;
  }

  DVLOG(1) << __func__ << " setting " << cdm_base_path << " to " << json_string;
  return base::ImportantFileWriter::WriteFileAtomically(hint_file_path,
                                                        json_string);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

base::FilePath GetHintedWidevineCdmDirectory() {
  base::Value::Dict dict = GetHintFileContents();

  auto* path_str = dict.FindString(kPath);
  if (!path_str) {
    DVLOG(1) << "CDM hint file missing " << kPath;
    return base::FilePath();
  }

  const base::FilePath path(*path_str);
  DLOG_IF(ERROR, path.empty())
      << "CDM hint file path " << *path_str << " is invalid.";
  DVLOG(1) << __func__ << " returns " << path;
  return path;
}

std::optional<base::Version> GetBundledVersionDuringLastComponentUpdate() {
  base::Value::Dict dict = GetHintFileContents();

  auto* version_str = dict.FindString(kLastBundledVersion);
  if (!version_str) {
    DVLOG(1) << "CDM hint file missing " << kLastBundledVersion;
    return std::nullopt;
  }

  const base::Version version(*version_str);
  DLOG_IF(ERROR, !version.IsValid())
      << "CDM hint file version " << *version_str << " is invalid.";
  DVLOG(1) << __func__ << " returns " << version;
  return version;
}
