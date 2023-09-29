// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/ephemeral_network_policies_enablement_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class EphemeralNetworkPoliciesEnablementHandlerTest : public ::testing::Test {
 public:
  EphemeralNetworkPoliciesEnablementHandlerTest() {
    policy_util::ResetEphemeralNetworkPoliciesEnabledForTesting();
    device_prefs_.registry()->RegisterBooleanPref(
        prefs::kDeviceEphemeralNetworkPoliciesEnabled, false);
  }

 protected:
  std::unique_ptr<EphemeralNetworkPoliciesEnablementHandler>
  CreateEphemeralNetworkPoliciesEnablementHandler() {
    return std::make_unique<EphemeralNetworkPoliciesEnablementHandler>(
        base::BindOnce(&EphemeralNetworkPoliciesEnablementHandlerTest::
                           OnEphemeralNetworkPoliciesEnabled,
                       base::Unretained(this)));
  }

  base::test::TaskEnvironment task_environment_;

  TestingPrefServiceSimple device_prefs_;

  bool ephemeral_network_policies_enabled_called_ = false;

  // Note that this can only ever be called once because it's bound using
  // BindOnce, resulting in a OnceCallback.
  void OnEphemeralNetworkPoliciesEnabled() {
    ephemeral_network_policies_enabled_called_ = true;
  }
};

TEST_F(EphemeralNetworkPoliciesEnablementHandlerTest, Disabled) {
  auto handler = CreateEphemeralNetworkPoliciesEnablementHandler();

  EXPECT_FALSE(policy_util::AreEphemeralNetworkPoliciesEnabled());

  handler->SetDevicePrefs(&device_prefs_);

  EXPECT_FALSE(ash::policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_FALSE(ephemeral_network_policies_enabled_called_);

  handler->SetDevicePrefs(nullptr);
}

TEST_F(EphemeralNetworkPoliciesEnablementHandlerTest, EnabledByFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEphemeralNetworkPolicies);

  auto handler = CreateEphemeralNetworkPoliciesEnablementHandler();

  EXPECT_TRUE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_TRUE(ephemeral_network_policies_enabled_called_);

  // Setting prefs doesn't change anything anymore.
  handler->SetDevicePrefs(&device_prefs_);

  EXPECT_TRUE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_TRUE(ephemeral_network_policies_enabled_called_);

  handler->SetDevicePrefs(nullptr);
}

TEST_F(EphemeralNetworkPoliciesEnablementHandlerTest, EnabledByPref_OnInit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEphemeralNetworkPoliciesEnabledPolicy},
      /*disabled_features=*/{features::kEphemeralNetworkPolicies});

  auto handler = CreateEphemeralNetworkPoliciesEnablementHandler();

  EXPECT_FALSE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_FALSE(ephemeral_network_policies_enabled_called_);

  device_prefs_.SetManagedPref(prefs::kDeviceEphemeralNetworkPoliciesEnabled,
                               base::Value(true));

  handler->SetDevicePrefs(&device_prefs_);

  EXPECT_TRUE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_TRUE(ephemeral_network_policies_enabled_called_);

  // Going back to false doesn't change the decision.
  device_prefs_.SetManagedPref(prefs::kDeviceEphemeralNetworkPoliciesEnabled,
                               base::Value(false));

  EXPECT_TRUE(policy_util::AreEphemeralNetworkPoliciesEnabled());

  handler->SetDevicePrefs(nullptr);
}

TEST_F(EphemeralNetworkPoliciesEnablementHandlerTest, EnabledByPref_AfterInit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEphemeralNetworkPoliciesEnabledPolicy},
      /*disabled_features=*/{features::kEphemeralNetworkPolicies});

  auto handler = CreateEphemeralNetworkPoliciesEnablementHandler();

  EXPECT_FALSE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_FALSE(ephemeral_network_policies_enabled_called_);

  handler->SetDevicePrefs(&device_prefs_);

  EXPECT_FALSE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_FALSE(ephemeral_network_policies_enabled_called_);

  device_prefs_.SetManagedPref(prefs::kDeviceEphemeralNetworkPoliciesEnabled,
                               base::Value(true));

  EXPECT_TRUE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_TRUE(ephemeral_network_policies_enabled_called_);

  // Going back to false doesn't change the decision.
  device_prefs_.SetManagedPref(prefs::kDeviceEphemeralNetworkPoliciesEnabled,
                               base::Value(false));

  EXPECT_TRUE(policy_util::AreEphemeralNetworkPoliciesEnabled());

  handler->SetDevicePrefs(nullptr);
}

TEST_F(EphemeralNetworkPoliciesEnablementHandlerTest,
       EnabledByPref_NotRespected_KillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kEphemeralNetworkPolicies,
                             features::kEphemeralNetworkPoliciesEnabledPolicy});

  auto handler = CreateEphemeralNetworkPoliciesEnablementHandler();

  EXPECT_FALSE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_FALSE(ephemeral_network_policies_enabled_called_);

  device_prefs_.SetManagedPref(prefs::kDeviceEphemeralNetworkPoliciesEnabled,
                               base::Value(true));

  handler->SetDevicePrefs(&device_prefs_);

  EXPECT_FALSE(policy_util::AreEphemeralNetworkPoliciesEnabled());
  EXPECT_FALSE(ephemeral_network_policies_enabled_called_);

  // Going back to false doesn't change the decision.
  device_prefs_.SetManagedPref(prefs::kDeviceEphemeralNetworkPoliciesEnabled,
                               base::Value(false));

  EXPECT_FALSE(policy_util::AreEphemeralNetworkPoliciesEnabled());

  handler->SetDevicePrefs(nullptr);
}

}  // namespace ash
