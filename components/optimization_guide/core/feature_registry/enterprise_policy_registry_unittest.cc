// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"

#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using model_execution::prefs::ModelExecutionEnterprisePolicyValue;

class EnterprisePolicyRegistryTest : public testing::Test {
 public:
  EnterprisePolicyRegistryTest() = default;
  ~EnterprisePolicyRegistryTest() override = default;

  void TearDown() override {
    EnterprisePolicyRegistry::GetInstance().ClearForTesting();
  }
};

TEST_F(EnterprisePolicyRegistryTest, Register) {
  TestingPrefServiceSimple pref_service;
  EnterprisePolicyRegistry::GetInstance().Register("pref_name");
  EnterprisePolicyRegistry::GetInstance().RegisterProfilePrefs(
      pref_service.registry());
  auto value =
      static_cast<model_execution::prefs::ModelExecutionEnterprisePolicyValue>(
          pref_service.GetInteger("pref_name"));
  // The default value should be kAllow.
  EXPECT_EQ(model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow,
            value);
}

TEST_F(EnterprisePolicyRegistryTest, GetValue) {
  TestingPrefServiceSimple pref_service;
  EnterprisePolicyPref enterprise_policy("pref_name");
  EnterprisePolicyRegistry::GetInstance().Register(enterprise_policy.name());
  EnterprisePolicyRegistry::GetInstance().RegisterProfilePrefs(
      pref_service.registry());

  EXPECT_EQ(model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow,
            enterprise_policy.GetValue(&pref_service));
  pref_service.SetInteger(
      "pref_name",
      static_cast<int>(
          model_execution::prefs::ModelExecutionEnterprisePolicyValue::
              kAllowWithoutLogging));
  EXPECT_EQ(model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                kAllowWithoutLogging,
            enterprise_policy.GetValue(&pref_service));
}

}  // namespace optimization_guide
