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

namespace {

base::Value::List ConvertToBaseValueList(
    const std::vector<std::string>& string_vector) {
  base::Value::List result;
  for (const std::string& str : string_vector) {
    result.Append(str);
  }
  return result;
}

}  // namespace

TestToplevelApnData::TestToplevelApnData(
    std::optional<std::string> id,
    std::optional<std::string> access_point_name,
    std::optional<std::string> ip_type,
    std::optional<std::vector<std::string>> apn_types,
    std::optional<std::vector<std::string>> admin_apn_list_ids,
    std::optional<std::vector<std::string>> psim_admin_assigned_apn_ids,
    std::optional<std::vector<std::string>> admin_assigned_apn_ids)
    : id(id),
      access_point_name(access_point_name),
      ip_type(ip_type),
      apn_types(apn_types),
      admin_apn_list_ids(admin_apn_list_ids),
      psim_admin_assigned_apn_ids(psim_admin_assigned_apn_ids),
      admin_assigned_apn_ids(admin_assigned_apn_ids) {}

TestToplevelApnData::~TestToplevelApnData() = default;

TestToplevelApnData::TestToplevelApnData(const TestToplevelApnData& other)
    : id(other.id),
      access_point_name(other.access_point_name),
      ip_type(other.ip_type),
      apn_types(other.apn_types),
      admin_apn_list_ids(other.admin_apn_list_ids),
      psim_admin_assigned_apn_ids(other.psim_admin_assigned_apn_ids),
      admin_assigned_apn_ids(other.admin_assigned_apn_ids) {}

// Helper function to build an individual APN dictionary
base::Value::Dict BuildApnDict(TestToplevelApnData apn_data) {
  base::Value::Dict apn_dict;

  auto maybe_set_string = [&](const char* key,
                              const std::optional<std::string>& value) {
    if (value.has_value()) {
      apn_dict.Set(key, *value);
    }
  };

  maybe_set_string(::onc::cellular_apn::kId, apn_data.id);
  maybe_set_string(::onc::cellular_apn::kAccessPointName,
                   apn_data.access_point_name);
  maybe_set_string(::onc::cellular_apn::kIpType, apn_data.ip_type);

  if (apn_data.apn_types) {
    apn_dict.Set(::onc::cellular_apn::kApnTypes,
                 ConvertToBaseValueList(apn_data.apn_types.value()));
  }

  return apn_dict;
}

const std::string GenerateTopLevelWithCellularWithAPNAsJSON(
    TestToplevelApnData apn_data) {
  base::Value::Dict top_level_dict =
      test_utils::ReadTestDictionary("toplevel_cellular_no_apn.onc");

  // Locate the Cellular config within the top-level dictionary
  base::Value::List* network_configs =
      top_level_dict.FindList(::onc::toplevel_config::kNetworkConfigurations);
  DCHECK(network_configs);
  base::Value::Dict* cellular_network_config_dict =
      network_configs->front().GetIfDict();
  DCHECK(cellular_network_config_dict);
  base::Value::Dict* cellular_dict =
      cellular_network_config_dict->FindDict(::onc::network_config::kCellular);
  DCHECK(cellular_dict);
  cellular_dict->Set(::onc::cellular::kAPN, BuildApnDict(apn_data));
  if (apn_data.admin_assigned_apn_ids.has_value()) {
    cellular_dict->Set(
        ::onc::cellular::kAdminAssignedAPNIds,
        ConvertToBaseValueList(apn_data.admin_assigned_apn_ids.value()));
  }

  if (apn_data.admin_apn_list_ids.has_value()) {
    base::Value::List admin_apn_list;

    for (const std::string& apn_id : apn_data.admin_apn_list_ids.value()) {
      // Use BuildApnDict with the apn_id and optional values as null
      TestToplevelApnData admin_apn_data;
      admin_apn_data.id = apn_id;
      admin_apn_data.access_point_name = "test-access-point-name";
      admin_apn_list.Append(BuildApnDict(admin_apn_data));
    }
    top_level_dict.Set(::onc::toplevel_config::kAdminAPNList,
                       std::move(admin_apn_list));
  }

  if (apn_data.psim_admin_assigned_apn_ids.has_value()) {
    // Locate the Global network config within the top-level dictionary
    base::Value::Dict* global_network_config = top_level_dict.FindDict(
        ::onc::toplevel_config::kGlobalNetworkConfiguration);
    DCHECK(global_network_config);
    global_network_config->Set(
        ::onc::global_network_config::kPSIMAdminAssignedAPNIds,
        ConvertToBaseValueList(apn_data.psim_admin_assigned_apn_ids.value()));
  }

  // Serialize to JSON string
  std::string json_output;
  if (!base::JSONWriter::WriteWithOptions(
          top_level_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &json_output)) {
    LOG(ERROR) << "JSON serialization failed";
  }
  return json_output;
}

}  // namespace chromeos::onc::test_utils
