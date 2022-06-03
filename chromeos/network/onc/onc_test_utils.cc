// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_test_utils.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chromeos/test/chromeos_test_utils.h"

namespace chromeos {
namespace onc {
namespace test_utils {

namespace {

// The name of the component directory to get the test data from.
const char kNetworkComponentDirectory[] = "network";

}  // namespace

std::string ReadTestData(const std::string& filename) {
  base::ScopedAllowBlockingForTesting allow_io;
  base::FilePath path;
  if (!chromeos::test_utils::GetTestDataPath(kNetworkComponentDirectory,
                                             filename, &path)) {
    LOG(FATAL) << "Unable to get test data path for "
               << kNetworkComponentDirectory << "/" << filename;
    return "";
  }
  std::string result;
  base::ReadFileToString(path, &result);
  return result;
}

std::unique_ptr<base::DictionaryValue> ReadTestDictionary(
    const std::string& filename) {
  return base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(ReadTestDictionaryValue(filename)));
}

base::Value ReadTestDictionaryValue(const std::string& filename) {
  base::FilePath path;
  if (!chromeos::test_utils::GetTestDataPath(kNetworkComponentDirectory,
                                             filename, &path)) {
    LOG(FATAL) << "Unable to get test dictionary path for "
               << kNetworkComponentDirectory << "/" << filename;
    return base::Value();
  }

  JSONFileValueDeserializer deserializer(path,
                                         base::JSON_ALLOW_TRAILING_COMMAS);

  std::string error_message;
  std::unique_ptr<base::Value> content =
      deserializer.Deserialize(nullptr, &error_message);
  CHECK(content != nullptr) << "Couldn't json-deserialize file '" << filename
                            << "': " << error_message;

  CHECK(content->is_dict())
      << "File '" << filename
      << "' does not contain a dictionary as expected, but type "
      << content->type();
  return std::move(*content);
}

::testing::AssertionResult Equals(const base::Value* expected,
                                  const base::Value* actual) {
  CHECK(expected != nullptr);
  if (actual == nullptr)
    return ::testing::AssertionFailure() << "Actual value pointer is nullptr";

  if (*expected == *actual)
    return ::testing::AssertionSuccess() << "Values are equal";

  return ::testing::AssertionFailure() << "Values are unequal.\n"
                                       << "Expected value:\n"
                                       << *expected << "Actual value:\n"
                                       << *actual;
}

}  // namespace test_utils
}  // namespace onc
}  // namespace chromeos
