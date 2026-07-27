// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_merger.h"

#include <string>

#include "base/values.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::onc {
namespace {

namespace test_utils = ::chromeos::onc::test_utils;

// Checks that both dictionaries contain an entry at |path| with the same value.
::testing::AssertionResult HaveSameValueAt(const base::DictValue& a,
                                           const base::DictValue& b,
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
  base::DictValue user_;
  base::DictValue policy_;
  base::DictValue policy_without_recommended_;
  base::DictValue device_policy_;
  base::DictValue active_;

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
  base::DictValue merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "Type"));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "StaticIPConfig"));
}

TEST_F(ONCMergerTest, MandatoryValueAndNoUserValue) {
  base::DictValue merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "GUID"));
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "VPN.OpenVPN.Username"));
}

TEST_F(ONCMergerTest, MandatoryDictionaryAndNoUserValue) {
  base::DictValue merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, policy_without_recommended_,
                              "VPN.OpenVPN.ClientCertPattern"));
}

TEST_F(ONCMergerTest, UserValueOverwritesRecommendedValue) {
  base::DictValue merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, user_, "VPN.Host"));
}

TEST_F(ONCMergerTest, UserValueAndRecommendedUnset) {
  base::DictValue merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, user_, "VPN.OpenVPN.Password"));
}

TEST_F(ONCMergerTest, UserDictionaryAndNoPolicyValue) {
  base::DictValue merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &user_, nullptr);
  EXPECT_FALSE(merged.contains("ProxySettings"));
}

TEST_F(ONCMergerTest, MergeWithEmptyPolicyProhibitsEverything) {
  base::DictValue empty_dict;
  base::DictValue merged = MergeSettingsAndPoliciesToEffective(
      &empty_dict, nullptr, &user_, nullptr);
  EXPECT_TRUE(merged.empty());
}

TEST_F(ONCMergerTest, MergeWithoutPolicyAllowsAnything) {
  base::DictValue merged =
      MergeSettingsAndPoliciesToEffective(nullptr, nullptr, &user_, nullptr);
  EXPECT_TRUE(test_utils::Equals(&user_, &merged));
}

TEST_F(ONCMergerTest, MergeWithoutUserSettings) {
  base::DictValue empty_dict;
  base::DictValue merged;

  merged = MergeSettingsAndPoliciesToEffective(&policy_, nullptr, &empty_dict,
                                               nullptr);
  EXPECT_TRUE(test_utils::Equals(&policy_without_recommended_, &merged));

  merged =
      MergeSettingsAndPoliciesToEffective(&policy_, nullptr, nullptr, nullptr);
  EXPECT_TRUE(test_utils::Equals(&policy_without_recommended_, &merged));
}

TEST_F(ONCMergerTest, MandatoryUserPolicyOverwritesDevicePolicy) {
  base::DictValue merged = MergeSettingsAndPoliciesToEffective(
      &policy_, &device_policy_, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, policy_, "VPN.OpenVPN.Port"));
}

TEST_F(ONCMergerTest, MandatoryDevicePolicyOverwritesRecommendedUserPolicy) {
  base::DictValue merged = MergeSettingsAndPoliciesToEffective(
      &policy_, &device_policy_, &user_, nullptr);
  EXPECT_TRUE(HaveSameValueAt(merged, device_policy_, "VPN.OpenVPN.Username"));
}

TEST_F(ONCMergerTest, MergeToAugmented) {
  base::DictValue expected_augmented =
      test_utils::ReadTestDictionary("augmented_merge.json");
  base::DictValue merged = MergeSettingsAndPoliciesToAugmented(
      chromeos::onc::kNetworkConfigurationSignature, &policy_, &device_policy_,
      &user_, nullptr, &active_);
  EXPECT_TRUE(test_utils::Equals(&expected_augmented, &merged));
}

TEST_F(ONCMergerTest, MergeToAugmentedWireGuardPeers) {
  base::DictValue policy =
      test_utils::ReadTestDictionary("managed_wireguard.onc");
  base::DictValue expected_augmented =
      test_utils::ReadTestDictionary("augmented_merge_wireguard.json");
  base::DictValue merged = MergeSettingsAndPoliciesToAugmented(
      chromeos::onc::kNetworkConfigurationSignature, &policy, nullptr, nullptr,
      nullptr, nullptr);
  EXPECT_TRUE(test_utils::Equals(&expected_augmented, &merged));
}

TEST_F(ONCMergerTest, MergeToAugmentedCellularApnList) {
  base::DictValue policy =
      test_utils::ReadTestDictionary("managed_cellular_apnlist.onc");
  base::DictValue expected_augmented =
      test_utils::ReadTestDictionary("augmented_merge_cellular_apnlist.json");
  base::DictValue merged = MergeSettingsAndPoliciesToAugmented(
      chromeos::onc::kNetworkConfigurationSignature, nullptr, &policy, nullptr,
      nullptr, nullptr);
  EXPECT_TRUE(test_utils::Equals(&expected_augmented, &merged));
}

TEST_F(ONCMergerTest, MergeToAugmentedCellularApnCredentials) {
  // The active settings contain both the configurable APN and the read-only
  // state APN dictionaries (LastGoodAPN, LastConnected*ApnProperty). All of
  // them carry an APN Password which must be replaced with the fake
  // credential placeholder in the augmented result.
  auto make_apn = [] {
    base::DictValue apn;
    apn.Set(::onc::cellular_apn::kAccessPointName, "internet");
    apn.Set(::onc::cellular_apn::kUsername, "user");
    apn.Set(::onc::cellular_apn::kPassword, "apn-password");
    return apn;
  };

  base::DictValue cellular;
  cellular.Set(::onc::cellular::kAPN, make_apn());
  cellular.Set(::onc::cellular::kLastGoodAPN, make_apn());
  cellular.Set(::onc::cellular::kLastConnectedAttachApnProperty, make_apn());
  cellular.Set(::onc::cellular::kLastConnectedDefaultApnProperty, make_apn());

  base::DictValue active;
  active.Set(::onc::network_config::kGUID, "cellular-guid");
  active.Set(::onc::network_config::kType, ::onc::network_type::kCellular);
  active.Set(::onc::network_config::kCellular, std::move(cellular));

  base::DictValue merged = MergeSettingsAndPoliciesToAugmented(
      chromeos::onc::kNetworkConfigurationSignature, /*user_policy=*/nullptr,
      /*device_policy=*/nullptr, /*user_settings=*/nullptr,
      /*shared_settings=*/nullptr, &active);

  // Cellular.APN is part of the configuration signature so its fields are
  // augmented and the password is masked.
  EXPECT_EQ(policy_util::kFakeCredential,
            *merged.FindStringByDottedPath("Cellular.APN.Password.Active"));

  // The state-only APN dictionaries are returned as plain values; their
  // passwords must still be masked while non-credential fields are preserved.
  for (const char* key : {::onc::cellular::kLastGoodAPN,
                          ::onc::cellular::kLastConnectedAttachApnProperty,
                          ::onc::cellular::kLastConnectedDefaultApnProperty}) {
    const base::DictValue* apn =
        merged.FindDict(::onc::network_config::kCellular)->FindDict(key);
    ASSERT_TRUE(apn) << key;
    EXPECT_EQ("internet",
              *apn->FindString(::onc::cellular_apn::kAccessPointName))
        << key;
    EXPECT_EQ(policy_util::kFakeCredential,
              *apn->FindString(::onc::cellular_apn::kPassword))
        << key;
  }
}

}  // namespace merger
}  // namespace ash::onc
