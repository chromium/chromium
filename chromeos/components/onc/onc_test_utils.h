// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_
#define CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::onc::test_utils {

class TestToplevelApnData {
 public:
  // Constructor
  explicit TestToplevelApnData(
      std::optional<std::string> id = std::nullopt,
      std::optional<std::string> access_point_name = "test-access-point-name",
      std::optional<std::string> ip_type = std::nullopt,
      std::optional<std::vector<std::string>> apn_types = std::nullopt,
      std::optional<std::vector<std::string>> admin_apn_list_ids = std::nullopt,
      std::optional<std::vector<std::string>> psim_admin_assigned_apn_ids =
          std::nullopt,
      std::optional<std::vector<std::string>> admin_assigned_apn_ids =
          std::nullopt);

  TestToplevelApnData(const TestToplevelApnData& other);
  ~TestToplevelApnData();

  std::optional<std::string> id;
  std::optional<std::string> access_point_name;
  std::optional<std::string> ip_type;
  std::optional<std::vector<std::string>> apn_types;
  std::optional<std::vector<std::string>> admin_apn_list_ids;
  std::optional<std::vector<std::string>> psim_admin_assigned_apn_ids;
  std::optional<std::vector<std::string>> admin_assigned_apn_ids;
};

// Read the file at |filename| as a string. CHECKs if any error occurs.
std::string ReadTestData(const std::string& filename);

// Read a JSON dictionary from |filename| and return it as a base::Value::Dict.
// CHECKs if any error occurs.
base::Value::Dict ReadTestDictionary(const std::string& filename);

// Read a JSON dictionary from |filename| and return it as a base::Value::List.
// CHECKs if any error occurs.
base::Value::List ReadTestList(const std::string& filename);

// Checks that the pointer |actual| is not NULL but points to a value that
// equals |expected|. The intended use case is:
// EXPECT_TRUE(test_utils::Equals(expected, actual));
::testing::AssertionResult Equals(const base::Value::Dict* expected,
                                  const base::Value::Dict* actual);

base::Value::Dict BuildApnDict(TestToplevelApnData apn_data);

base::Value::List BuildAdminApnList(
    const std::vector<std::string>& admin_apn_list_ids);

// Generates a JSON string representing a top-level Open Network
// Configuration (ONC) dictionary with a modified APN in the first Network
// Configuration which must be of type Cellular.
//
// The Cellular configuration is updated with the provided APN (Access Point
// Name) details, allowing optional customization of the access point name,
// IP type, and APN types.
//
// Returns a JSON string representation of the modified ONC dictionary, or
// an empty string if an error occurred during serialization.
const std::string GenerateTopLevelWithCellularWithAPNAsJSON(
    TestToplevelApnData apn_data = TestToplevelApnData());
}  // namespace chromeos::onc::test_utils

#endif  // CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_
