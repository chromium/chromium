// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_merger.h"

#include <string>

#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {
namespace {

// Checks that both dictionaries contain an entry at |path| with the same value.
::testing::AssertionResult HaveSameValueAt(const base::DictionaryValue& a,
                                           const base::DictionaryValue& b,
                                           const std::string& path) {
  const base::Value* a_value = NULL;
  if (!a.Get(path, &a_value)) {
    return ::testing::AssertionFailure()
        << "First dictionary '" << a << "' doesn't contain " << path;
  }

  const base::Value* b_value = NULL;
  if (!b.Get(path, &b_value)) {
    return ::testing::AssertionFailure()
        << "Second dictionary '" << b << "' doesn't contain " << path;
  }

  if (*a_value == *b_value) {
    return ::testing::AssertionSuccess()
        << "Entries at '" << path << "' are equal";
  } else {
    return ::testing::AssertionFailure()
        << "Entries at '" << path << "' not equal but are '"
        << *a_value << "' and '" << *b_value << "'";
  }
}

}  // namespace

namespace merger {

class ONCMergerTest : public testing::Test {
 public:
  std::unique_ptr<const base::DictionaryValue> user_;
  std::unique_ptr<const base::DictionaryValue> policy_;
  std::unique_ptr<const base::DictionaryValue> policy_without_recommended_;
  std::unique_ptr<const base::DictionaryValue> device_policy_;
  std::unique_ptr<const base::DictionaryValue> active_;

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
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(policy_.get(), NULL, user_.get(),
                                          NULL));
  EXPECT_TRUE(HaveSameValueAt(*merged, *policy_, "Type"));
  EXPECT_TRUE(HaveSameValueAt(*merged, *policy_, "StaticIPConfig"));
}

TEST_F(ONCMergerTest, MandatoryValueAndNoUserValue) {
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(policy_.get(), NULL, user_.get(),
                                          NULL));
  EXPECT_TRUE(HaveSameValueAt(*merged, *policy_, "GUID"));
  EXPECT_TRUE(HaveSameValueAt(*merged, *policy_, "VPN.OpenVPN.Username"));
}

TEST_F(ONCMergerTest, MandatoryDictionaryAndNoUserValue) {
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(policy_.get(), NULL, user_.get(),
                                          NULL));
  EXPECT_TRUE(HaveSameValueAt(*merged, *policy_without_recommended_,
                              "VPN.OpenVPN.ClientCertPattern"));
}

TEST_F(ONCMergerTest, UserValueOverwritesRecommendedValue) {
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(policy_.get(), NULL, user_.get(),
                                          NULL));
  EXPECT_TRUE(HaveSameValueAt(*merged, *user_, "VPN.Host"));
}

TEST_F(ONCMergerTest, UserValueAndRecommendedUnset) {
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(policy_.get(), NULL, user_.get(),
                                          NULL));
  EXPECT_TRUE(HaveSameValueAt(*merged, *user_, "VPN.OpenVPN.Password"));
}

TEST_F(ONCMergerTest, UserDictionaryAndNoPolicyValue) {
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(policy_.get(), NULL, user_.get(),
                                          NULL));
  const base::Value* value = NULL;
  EXPECT_FALSE(merged->Get("ProxySettings", &value));
}

TEST_F(ONCMergerTest, MergeWithEmptyPolicyProhibitsEverything) {
  base::DictionaryValue emptyDict;
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(&emptyDict, NULL, user_.get(), NULL));
  EXPECT_TRUE(merged->DictEmpty());
}

TEST_F(ONCMergerTest, MergeWithoutPolicyAllowsAnything) {
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(NULL, NULL, user_.get(), NULL));
  EXPECT_TRUE(test_utils::Equals(user_.get(), merged.get()));
}

TEST_F(ONCMergerTest, MergeWithoutUserSettings) {
  base::DictionaryValue emptyDict;
  std::unique_ptr<base::DictionaryValue> merged;

  merged = MergeSettingsAndPoliciesToEffective(
      policy_.get(), NULL, &emptyDict, NULL);
  EXPECT_TRUE(test_utils::Equals(policy_without_recommended_.get(),
                                 merged.get()));

  merged = MergeSettingsAndPoliciesToEffective(policy_.get(), NULL, NULL, NULL);
  EXPECT_TRUE(test_utils::Equals(policy_without_recommended_.get(),
                                 merged.get()));
}

TEST_F(ONCMergerTest, MandatoryUserPolicyOverwritesDevicePolicy) {
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(policy_.get(), device_policy_.get(),
                                          user_.get(), NULL));
  EXPECT_TRUE(HaveSameValueAt(*merged, *policy_, "VPN.OpenVPN.Port"));
}

TEST_F(ONCMergerTest, MandatoryDevicePolicyOverwritesRecommendedUserPolicy) {
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToEffective(policy_.get(), device_policy_.get(),
                                          user_.get(), NULL));
  EXPECT_TRUE(HaveSameValueAt(*merged, *device_policy_,
                              "VPN.OpenVPN.Username"));
}

TEST_F(ONCMergerTest, MergeToAugmented) {
  std::unique_ptr<base::DictionaryValue> expected_augmented =
      test_utils::ReadTestDictionary("augmented_merge.json");
  std::unique_ptr<base::DictionaryValue> merged(
      MergeSettingsAndPoliciesToAugmented(kNetworkConfigurationSignature,
                                          policy_.get(), device_policy_.get(),
                                          user_.get(), NULL, active_.get()));
  EXPECT_TRUE(test_utils::Equals(expected_augmented.get(), merged.get()));
}

}  // namespace merger
}  // namespace onc
}  // namespace chromeos
