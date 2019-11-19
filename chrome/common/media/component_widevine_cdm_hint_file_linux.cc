// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"

namespace {

// Fields used inside the hint file.
const char kPath[] = "Path";

base::FilePath GetPath(const base::Value& dict) {
  DCHECK(dict.is_dict());

  auto* path_str = dict.FindStringKey(kPath);
  if (!path_str) {
    DLOG(ERROR) << "CDM hint file missing " << kPath;
    return base::FilePath();
  }

  const base::FilePath path(*path_str);
  DLOG_IF(ERROR, path.empty())
      << "CDM hint file path " << path_str << " is invalid.";
  return path;
}

}  // namespace

bool UpdateWidevineCdmHintFile(const base::FilePath& cdm_base_path) {
  DCHECK(!cdm_base_path.empty());

  base::FilePath hint_file_path;
  CHECK(base::PathService::Get(chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT,
                               &hint_file_path));

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringPath(kPath, cdm_base_path.value());

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  if (!serializer.Serialize(dict)) {
    DLOG(ERROR) << "Could not serialize the CDM hint file.";
    return false;
  }

  return base::ImportantFileWriter::WriteFileAtomically(hint_file_path,
                                                        json_string);
}

base::FilePath GetLatestComponentUpdatedWidevineCdmDirectory() {
  base::FilePath hint_file_path;
  CHECK(base::PathService::Get(chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT,
                               &hint_file_path));

  if (!base::PathExists(hint_file_path)) {
    DVLOG(2) << "CDM hint file at " << hint_file_path << " does not exist.";
    return base::FilePath();
  }

  std::string json_string;
  if (!base::ReadFileToString(hint_file_path, &json_string)) {
    DLOG(ERROR) << "Could not read the CDM hint file at " << hint_file_path;
    return base::FilePath();
  }

  std::string error_message;
  JSONStringValueDeserializer deserializer(json_string);
  std::unique_ptr<base::Value> dict =
      deserializer.Deserialize(/*error_code=*/nullptr, &error_message);

  if (!dict || !dict->is_dict()) {
    DLOG(ERROR) << "Could not deserialize the CDM hint file. Error: "
                << error_message;
    return base::FilePath();
  }

  return GetPath(*dict);
}
