// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/serializers.h"

#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"

namespace chromecast {

std::unique_ptr<base::Value> DeserializeFromJson(const std::string& text) {
  JSONStringValueDeserializer deserializer(text);

  int error_code = -1;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  DLOG_IF(ERROR, !value) << "JSON error " << error_code << ":" << error_msg;

  // Value will hold the nullptr in case of an error.
  return value;
}

base::Optional<std::string> SerializeToJson(const base::Value& value) {
  std::string json_str;
  JSONStringValueSerializer serializer(&json_str);
  if (serializer.Serialize(value))
    return json_str;
  return base::nullopt;
}

std::unique_ptr<base::Value> DeserializeJsonFromFile(
    const base::FilePath& path) {
  JSONFileValueDeserializer deserializer(path);

  int error_code = -1;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  DLOG_IF(ERROR, !value) << "JSON error " << error_code << ":" << error_msg;

  // Value will hold the nullptr in case of an error.
  return value;
}

bool SerializeJsonToFile(const base::FilePath& path, const base::Value& value) {
  JSONFileValueSerializer serializer(path);
  return serializer.Serialize(value);
}

}  // namespace chromecast
