// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {


class EnterprisePolicyRegistryTest : public testing::Test {};

TEST_F(EnterprisePolicyRegistryTest, Register) {
  TestingPrefServiceSimple pref_service;
  std::unique_ptr<EnterprisePolicyRegistry> registry =
      EnterprisePolicyRegistry::CreateForTesting();
  registry->Register("pref_name");
  registry->RegisterProfilePrefs(pref_service.registry());
  auto value =
      static_cast<EnterprisePolicyValue>(pref_service.GetInteger("pref_name"));
  // The default value should be kAllow.
  EXPECT_EQ(EnterprisePolicyValue::kAllow, value);
}

TEST_F(EnterprisePolicyRegistryTest, GetValue) {
  TestingPrefServiceSimple pref_service;
  std::unique_ptr<EnterprisePolicyRegistry> registry =
      EnterprisePolicyRegistry::CreateForTesting();
  EnterprisePolicyPref enterprise_policy = registry->Register("pref_name");
  registry->RegisterProfilePrefs(pref_service.registry());

  EXPECT_EQ(EnterprisePolicyValue::kAllow,
            enterprise_policy.GetValue(&pref_service));
  pref_service.SetInteger(
      "pref_name",
      static_cast<int>(EnterprisePolicyValue::kAllowWithoutLogging));
  EXPECT_EQ(EnterprisePolicyValue::kAllowWithoutLogging,
            enterprise_policy.GetValue(&pref_service));
}

}  // namespace optimization_guide
