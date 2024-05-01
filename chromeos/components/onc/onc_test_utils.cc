// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_test_utils.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"

namespace chromeos::onc::test_utils {

namespace {

bool GetTestDataPath(const std::string& filename, base::FilePath* result_path) {
  base::ScopedAllowBlockingForTesting allow_io;

  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path)) {
    LOG(FATAL) << "Failed to get the path to root for " << filename;
  }
  path = path.Append(FILE_PATH_LITERAL("chromeos"));
  path = path.Append(FILE_PATH_LITERAL("components"));
  path = path.Append(FILE_PATH_LITERAL("test"));
  path = path.Append(FILE_PATH_LITERAL("data"));
  path = path.Append(FILE_PATH_LITERAL("onc"));
  path = path.Append(FILE_PATH_LITERAL(filename));
  if (!base::PathExists(path)) {  // We don't want to create this.
    LOG(FATAL) << "The file doesn't exist: " << path;
  }

  *result_path = path;
  return true;
}

}  // namespace

std::string ReadTestData(const std::string& filename) {
  base::ScopedAllowBlockingForTesting allow_io;
  base::FilePath path;
  if (!GetTestDataPath(filename, &path)) {
    return "";
  }
  std::string result;
  base::ReadFileToString(path, &result);
  return result;
}

namespace {

base::Value ReadTestJson(const std::string& filename) {
  base::FilePath path;
  if (!GetTestDataPath(filename, &path)) {
    LOG(FATAL) << "Unable to get test file path for: " << filename;
  }
  JSONFileValueDeserializer deserializer(
      path,
      base::JSON_PARSE_CHROMIUM_EXTENSIONS | base::JSON_ALLOW_TRAILING_COMMAS);
  std::string error_message;
  std::unique_ptr<base::Value> result =
      deserializer.Deserialize(nullptr, &error_message);
  CHECK(result != nullptr) << "Couldn't json-deserialize file: " << filename
                           << ": " << error_message;
  return std::move(*result);
}

}  // namespace

base::Value::Dict ReadTestDictionary(const std::string& filename) {
  base::Value content = ReadTestJson(filename);
  CHECK(content.is_dict())
      << "File '" << filename
      << "' does not contain a dictionary as expected, but type "
      << content.type();
  return std::move(content.GetDict());
}

base::Value::List ReadTestList(const std::string& filename) {
  base::Value content = ReadTestJson(filename);
  CHECK(content.is_list()) << "File '" << filename
                           << "' does not contain a list as expected, but type "
                           << content.type();
  return std::move(content.GetList());
}

::testing::AssertionResult Equals(const base::Value::Dict* expected,
                                  const base::Value::Dict* actual) {
  CHECK(expected != nullptr);
  if (actual == nullptr) {
    return ::testing::AssertionFailure() << "Actual value pointer is nullptr";
  }

  if (*expected == *actual) {
    return ::testing::AssertionSuccess() << "Values are equal";
  }

  return ::testing::AssertionFailure() << "Values are unequal.\n"
                                       << "Expected value:\n"
                                       << *expected << "Actual value:\n"
                                       << *actual;
}

const std::string GenerateTopLevelWithCellularWithAPNAsJson(
    const std::optional<std::string>& access_point_name,
    const std::optional<std::string>& ip_type,
    const std::optional<std::vector<std::string>>& apn_types) {
  base::Value::Dict top_level =
      test_utils::ReadTestDictionary("toplevel_cellular_no_apn.onc");

  // Helper function to set optional properties
  auto maybe_set_value = [](base::Value::Dict& dict, const char* key,
                            const std::optional<std::string>& value) {
    if (value) {
      dict.Set(key, *value);
    }
  };

  // Construct APN dictionary
  base::Value::Dict apn_dict;
  maybe_set_value(apn_dict, ::onc::cellular_apn::kAccessPointName,
                  access_point_name);
  maybe_set_value(apn_dict, ::onc::cellular_apn::kIpType, ip_type);

  // Handle apn_types, including nullopt and empty list
  if (apn_types.has_value()) {
    base::Value::List apn_types_list = base::Value::List();
    for (const std::string& type : *apn_types) {
      apn_types_list.Append(type);
    }
    apn_dict.Set(::onc::cellular_apn::kApnTypes, std::move(apn_types_list));
  }

  // Find and update the cellular configuration
  base::Value::List* network_configs =
      top_level.FindList(::onc::toplevel_config::kNetworkConfigurations);
  DCHECK(network_configs);
  base::Value::Dict* cellular_network_config_dict =
      network_configs->front().GetIfDict();
  DCHECK(cellular_network_config_dict);
  base::Value::Dict* cellular_dict =
      cellular_network_config_dict->FindDict(::onc::network_config::kCellular);
  DCHECK(cellular_dict);
  cellular_dict->Set(::onc::cellular::kAPN, std::move(apn_dict));

  // Serialize to JSON string
  std::string json_output;
  if (!base::JSONWriter::WriteWithOptions(
          top_level, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_output)) {
    LOG(ERROR) << "JSON serialization failed";
  }
  return json_output;
}

}  // namespace chromeos::onc::test_utils
