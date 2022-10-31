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
::testing::AssertionResult HaveSameValueAt(const base::Value& a,
                                           const base::Value& b,
                                           const std::string& path) {
  const base::Value* a_value = a.FindPath(path);
  if (!a_value) {
    return ::testing::AssertionFailure()
           << "First dictionary '" << a << "' doesn't contain " << path;
  }

  const base::Value* b_value = b.FindPath(path);
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
  base::Value user_;
  base::Value policy_;
  base::Value policy_without_recommended_;
  base::Value device_policy_;
  base::Value active_;

  void SetUp() override {
    policy_ = test_utils::ReadTestDictionaryValue("managed_vpn.onc");
    policy_without_recommended_ = test_utils::ReadTestDictionaryValue(
        "managed_vpn_without_recommended.onc");
    user_ = test_utils::ReadTestDictionaryValue("user.onc");
    device_policy_ = test_utils::ReadTestDictionaryValue("device_policy.onc");
    active_ = test_utils::ReadTestDictionaryValue("vpn_active_settings.onc");
  }
};

TEST_F(ONCMergerTest, MandatoryValueOverwritesUserValue) {
  base::Value merged(
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "Type"));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "StaticIPConfig"));
}

TEST_F(ONCMergerTest, MandatoryValueAndNoUserValue) {
  base::Value merged(
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "GUID"));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "VPN.OpenVPN.Username"));
}

TEST_F(ONCMergerTest, MandatoryDictionaryAndNoUserValue) {
  base::Value merged(
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_without_recommended_,
                              "VPN.OpenVPN.ClientCertPattern"));
}

TEST_F(ONCMergerTest, UserValueOverwritesRecommendedValue) {
  base::Value merged(
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr));
  EXPECT_TRUE(HaveSameValueAt(merged, user_, "VPN.Host"));
}

TEST_F(ONCMergerTest, UserValueAndRecommendedUnset) {
  base::Value merged(
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr));
  EXPECT_TRUE(HaveSameValueAt(merged, user_, "VPN.OpenVPN.Password"));
}

TEST_F(ONCMergerTest, UserDictionaryAndNoPolicyValue) {
  base::Value merged(
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr));
  EXPECT_FALSE(merged.FindKey("ProxySettings"));
}

TEST_F(ONCMergerTest, MergeWithEmptyPolicyProhibitsEverything) {
  base::Value emptyDict(base::Value::Type::DICTIONARY);
  base::Value merged(MergeSettingsAndPoliciesToEffective(&emptyDict, nullptr,
                                                         &user_, nullptr));
  EXPECT_TRUE(merged.DictEmpty());
}

TEST_F(ONCMergerTest, MergeWithoutPolicyAllowsAnything) {
  base::Value merged(
      MergeSettingsAndPoliciesToEffective(nullptr, nullptr, &user_, nullptr));
  EXPECT_TRUE(test_utils::Equals(&user_, &merged));
}

TEST_F(ONCMergerTest, MergeWithoutUserSettings) {
  base::Value emptyDict(base::Value::Type::DICTIONARY);
  base::Value merged;

  merged = MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &emptyDict,
                                               nullptr);
  EXPECT_TRUE(test_utils::Equals(&policy_without_recommended_, &merged));

  merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, nullptr, nullptr);
  EXPECT_TRUE(test_utils::Equals(&policy_without_recommended_, &merged));
}

TEST_F(ONCMergerTest, MandatoryUserPolicyOverwritesDevicePolicy) {
  base::Value merged(MergeSettingsAndPoliciesToEffective(
      &policy_, &device_policy_, &user_, nullptr));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "VPN.OpenVPN.Port"));
}

TEST_F(ONCMergerTest, MandatoryDevicePolicyOverwritesRecommendedUserPolicy) {
  base::Value merged(MergeSettingsAndPoliciesToEffective(
      &policy_, &device_policy_, &user_, nullptr));
  EXPECT_TRUE(HaveSameValueAt(merged, device_policy_, "VPN.OpenVPN.Username"));
}

TEST_F(ONCMergerTest, MergeToAugmented) {
  base::Value expected_augmented =
      test_utils::ReadTestDictionaryValue("augmented_merge.json");
  base::Value merged(MergeSettingsAndPoliciesToAugmented(
      chromeos::onc::kNetworkConfigurationSignature, &policy_, &device_policy_,
      &user_, nullptr, &active_));
  EXPECT_TRUE(test_utils::Equals(&expected_augmented, &merged));
}

}  // namespace merger
}  // namespace ash::onc
