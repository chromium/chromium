// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/file_system_core_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace drive {
namespace util {

namespace {

std::string ReadStringFromGDocFile(const base::FilePath& file_path,
                                   const std::string& key) {
  const int64_t kMaxGDocSize = 4096;
  int64_t file_size = 0;
  if (!base::GetFileSize(file_path, &file_size) || file_size > kMaxGDocSize) {
    LOG(WARNING) << "File too large to be a GDoc file " << file_path.value();
    return std::string();
  }

  JSONFileValueDeserializer reader(file_path);
  std::string error_message;
  std::unique_ptr<base::Value> root_value(
      reader.Deserialize(nullptr, &error_message));
  if (!root_value) {
    LOG(WARNING) << "Failed to parse " << file_path.value() << " as JSON."
                 << " error = " << error_message;
    return std::string();
  }

  base::DictionaryValue* dictionary_value = nullptr;
  std::string result;
  if (!root_value->GetAsDictionary(&dictionary_value) ||
      !dictionary_value->GetString(key, &result)) {
    LOG(WARNING) << "No value for the given key is stored in "
                 << file_path.value() << ". key = " << key;
    return std::string();
  }

  return result;
}

}  // namespace

const base::FilePath& GetDriveGrandRootPath() {
  static base::NoDestructor<base::FilePath> grand_root_path(
      base::FilePath::FromUTF8Unsafe(kDriveGrandRootDirName));
  return *grand_root_path;
}

std::string ConvertChangestampToStartPageToken(int64_t changestamp) {
  DCHECK_LE(0, changestamp);
  return base::NumberToString(changestamp + 1);
}

GURL ReadUrlFromGDocFile(const base::FilePath& file_path) {
  return GURL(ReadStringFromGDocFile(file_path, "url"));
}

std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path) {
  return ReadStringFromGDocFile(file_path, "resource_id");
}

}  // namespace util
}  // namespace drive
