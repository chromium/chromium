// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/policy_util.h"

#include <optional>
#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::policy_util {
namespace {

typedef testing::Test PolicyUtilTest;

constexpr char kSmdpActivationCode[] = "smdp_activation_code";
constexpr char kSmdsActivationCode[] = "smds_activation_code";

}  // namespace

TEST_F(PolicyUtilTest, GetActivationCodesFromONC) {
  std::optional<SmdxActivationCode> activation_code;

  // Start with an empty configuration and slowly populate it.
  base::Value::Dict onc_config;

  auto expect_invalid = [&]() {
    activation_code = GetSmdxActivationCodeFromONC(onc_config);
    EXPECT_FALSE(activation_code.has_value());
  };

  expect_invalid();

  onc_config.Set(::onc::network_config::kType,
                 base::Value(::onc::network_type::kWiFi));
  expect_invalid();

  onc_config.Set(::onc::network_config::kType,
                 base::Value(::onc::network_type::kCellular));
  expect_invalid();

  base::Value::Dict cellular_dict;
  onc_config.Set(::onc::network_config::kCellular, cellular_dict.Clone());
  expect_invalid();

  cellular_dict.Set(::onc::cellular::kSMDPAddress,
                    base::Value(kSmdpActivationCode));
  onc_config.Set(::onc::network_config::kCellular, cellular_dict.Clone());

  activation_code = GetSmdxActivationCodeFromONC(onc_config);
  EXPECT_TRUE(activation_code.has_value());
  EXPECT_EQ(activation_code->type(), SmdxActivationCode::Type::SMDP);
  EXPECT_EQ(activation_code->value(), kSmdpActivationCode);

  cellular_dict.Set(::onc::cellular::kSMDSAddress,
                    base::Value(kSmdsActivationCode));
  onc_config.Set(::onc::network_config::kCellular, cellular_dict.Clone());

  // Only one activation code can be provided.
  expect_invalid();

  cellular_dict.Remove(::onc::cellular::kSMDPAddress);
  onc_config.Set(::onc::network_config::kCellular, cellular_dict.Clone());

  activation_code = GetSmdxActivationCodeFromONC(onc_config);
  EXPECT_TRUE(activation_code.has_value());
  EXPECT_EQ(activation_code->type(), SmdxActivationCode::Type::SMDS);
  EXPECT_EQ(activation_code->value(), kSmdsActivationCode);
}

TEST_F(PolicyUtilTest, HasAnyRecommendedField_TopLevel) {
  const char* const top_level_recommended = R"(
      {
        "Type": "WiFi",
        "Recommended": ["Test"]
      })";
  EXPECT_TRUE(
      HasAnyRecommendedField(base::test::ParseJsonDict(top_level_recommended)));

  const char* const top_level_recommended_empty = R"(
      {
        "Type": "WiFi",
        "Recommended": []
      })";
  EXPECT_FALSE(HasAnyRecommendedField(
      base::test::ParseJsonDict(top_level_recommended_empty)));

  const char* const top_level_not_recommended = R"(
      {
        "Type": "WiFi",
      })";
  EXPECT_FALSE(HasAnyRecommendedField(
      base::test::ParseJsonDict(top_level_not_recommended)));
}

TEST_F(PolicyUtilTest, HasAnyRecommendedField_Nested) {
  const char* const nested_recommended_level1 = R"(
      {
        "Subdict": {
          "Recommended": ["Test"],
        },
        "Type": "WiFi",
        "SomeList": [
          { },
        ]
      })";
  EXPECT_TRUE(HasAnyRecommendedField(
      base::test::ParseJsonDict(nested_recommended_level1)));

  const char* const nested_recommended_level2 = R"(
      {
        "Type": "WiFi",
        "SomeList": [
          { },
          {
            "Recommended": ["Test"]
          }
        ]
      })";
  EXPECT_TRUE(HasAnyRecommendedField(
      base::test::ParseJsonDict(nested_recommended_level2)));

  const char* const nested_recommended_level3 = R"(
      {
        "Type": "WiFi",
        "SomeList": [
          { },
          {
            "Nested": {
              "Recommended": ["Test"]
            }
          }
        ]
      })";
  EXPECT_TRUE(HasAnyRecommendedField(
      base::test::ParseJsonDict(nested_recommended_level3)));

  const char* const nested_recommended_empty = R"(
      {
        "Type": "WiFi",
        "SomeList": [
          { },
          {
            "Nested": {
              "Recommended": []
            }
          }
        ]
      })";
  EXPECT_FALSE(HasAnyRecommendedField(
      base::test::ParseJsonDict(nested_recommended_empty)));

  const char* const nested_recommended_not_a_list = R"(
      {
        "Type": "WiFi",
        "SomeList": [
          { },
          {
            "Nested": {
              "Recommended": "not_a_list"
            }
          }
        ]
      })";
  EXPECT_FALSE(HasAnyRecommendedField(
      base::test::ParseJsonDict(nested_recommended_not_a_list)));

  const char* const nested_not_recommended = R"(
      {
        "Type": "WiFi",
        "SomeList": [
          { },
          {
            "Nested": {
            }
          }
        ]
      })";
  EXPECT_FALSE(HasAnyRecommendedField(
      base::test::ParseJsonDict(nested_not_recommended)));
}

}  // namespace ash::policy_util
