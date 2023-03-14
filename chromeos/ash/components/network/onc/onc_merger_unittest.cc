// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_merger.h"

#include <string>

#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::onc {
namespace {

namespace test_utils = ::chromeos::onc::test_utils;

// Checks that both dictionaries contain an entry at |path| with the same value.
::testing::AssertionResult HaveSameValueAt(const base::Value::Dict& a,
                                           const base::Value::Dict& b,
                                           const std::string& path) {
  const base::Value* a_value = a.FindByDottedPath(path);
  if (!a_value) {
    return ::testing::AssertionFailure()
           << "First dictionary '" << a << "' doesn't contain " << path;
  }

  const base::Value* b_value = b.FindByDottedPath(path);
  if (!b_value) {
    return ::testing::AssertionFailure()
           << "Second dictionary '" << b << "' doesn't contain " << path;
  }

  if (*a_value == *b_value) {
    return ::testing::AssertionSuccess()
           << "Entries at '" << path << "' are equal";
  } else {
    return ::testing::AssertionFailure()
           << "Entries at '" << path << "' not equal but are '" << *a_value
           << "' and '" << *b_value << "'";
  }
}

}  // namespace

namespace merger {

class ONCMergerTest : public testing::Test {
 public:
  base::Value::Dict user_;
  base::Value::Dict policy_;
  base::Value::Dict policy_without_recommended_;
  base::Value::Dict device_policy_;
  base::Value::Dict active_;

  void SetUp() override {
    policy_ = test_utils::ReadTestDictionary("managed_vpn.onc");
    policy_without_recommended_ =
        test_utils::ReadTestDictionary("managed_vpn_without_recommended.onc");
    user_ = test_utils::ReadTestDictionary("user.onc");
    device_policy_ = test_utils::ReadTestDictionary("device_policy.onc");
    active_ = test_utils::ReadTestDictionary("vpn_active_settings.onc");
  }
};

TEST_F(ONCMergerTest, MandatoryValueOverwritesUserValue) {
  base::Value::Dict merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "Type"));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "StaticIPConfig"));
}

TEST_F(ONCMergerTest, MandatoryValueAndNoUserValue) {
  base::Value::Dict merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "GUID"));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "VPN.OpenVPN.Username"));
}

TEST_F(ONCMergerTest, MandatoryDictionaryAndNoUserValue) {
  base::Value::Dict merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, policy_without_recommended_,
                              "VPN.OpenVPN.ClientCertPattern"));
}

TEST_F(ONCMergerTest, UserValueOverwritesRecommendedValue) {
  base::Value::Dict merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, user_, "VPN.Host"));
}

TEST_F(ONCMergerTest, UserValueAndRecommendedUnset) {
  base::Value::Dict merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, user_, "VPN.OpenVPN.Password"));
}

TEST_F(ONCMergerTest, UserDictionaryAndNoPolicyValue) {
  base::Value::Dict merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_FALSE(merged.contains("ProxySettings"));
}

TEST_F(ONCMergerTest, MergeWithEmptyPolicyProhibitsEverything) {
  base::Value::Dict empty_dict;
  base::Value::Dict merged = MergeSettingsAndPoliciesToEffective(
      &empty_dict, nullptr, &user_, nullptr);
  EXPECT_TRUE(merged.empty());
}

TEST_F(ONCMergerTest, MergeWithoutPolicyAllowsAnything) {
  base::Value::Dict merged =
      MergeSettingsAndPoliciesToEffective(nullptr, nullptr, &user_, nullptr);
  EXPECT_TRUE(test_utils::Equals(&user_, &merged));
}

TEST_F(ONCMergerTest, MergeWithoutUserSettings) {
  base::Value::Dict empty_dict;
  base::Value::Dict merged;

  merged = MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &empty_dict,
                                               nullptr);
  EXPECT_TRUE(test_utils::Equals(&policy_without_recommended_, &merged));

  merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, nullptr, nullptr);
  EXPECT_TRUE(test_utils::Equals(&policy_without_recommended_, &merged));
}

TEST_F(ONCMergerTest, MandatoryUserPolicyOverwritesDevicePolicy) {
  base::Value::Dict merged = MergeSettingsAndPoliciesToEffective(
      &policy_, &device_policy_, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "VPN.OpenVPN.Port"));
}

TEST_F(ONCMergerTest, MandatoryDevicePolicyOverwritesRecommendedUserPolicy) {
  base::Value::Dict merged = MergeSettingsAndPoliciesToEffective(
      &policy_, &device_policy_, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, device_policy_, "VPN.OpenVPN.Username"));
}

TEST_F(ONCMergerTest, MergeToAugmented) {
  base::Value::Dict expected_augmented =
      test_utils::ReadTestDictionary("augmented_merge.json");
  base::Value::Dict merged = MergeSettingsAndPoliciesToAugmented(
      chromeos::onc::kNetworkConfigurationSignature, &policy_, &device_policy_,
      &user_, nullptr, &active_);
  EXPECT_TRUE(test_utils::Equals(&expected_augmented, &merged));
}

}  // namespace merger
}  // namespace ash::onc
