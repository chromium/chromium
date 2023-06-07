// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/boolean_disabling_policy_handler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

const char kTestPolicy[] = "unit_test.test_policy";
const char kTestPref[] = "unit_test.test_pref";

class BooleanDisablingPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    handler_list_.AddHandler(std::make_unique<BooleanDisablingPolicyHandler>(
        kTestPolicy, kTestPref));
  }
};

TEST_F(BooleanDisablingPolicyHandlerTest, PolicyNotSet) {
  const base::Value* value_ptr = nullptr;
  PolicyMap policy;
  UpdateProviderPolicy(policy);
  // When no policy is set, no value should be pushed to prefs.
  EXPECT_FALSE(store_->GetValue(kTestPref, &value_ptr));
}

TEST_F(BooleanDisablingPolicyHandlerTest, PolicySetToTrue) {
  const base::Value* value_ptr = nullptr;
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  UpdateProviderPolicy(policy);
  // When policy is true, no value should be pushed to prefs.
  EXPECT_FALSE(store_->GetValue(kTestPref, &value_ptr));
}

TEST_F(BooleanDisablingPolicyHandlerTest, PolicySetToFalse) {
  const base::Value* value_ptr = nullptr;
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policy);
  // When policy is false, false should be pushed to prefs.
  EXPECT_TRUE(store_->GetValue(kTestPref, &value_ptr));
  EXPECT_FALSE(value_ptr->GetBool());
}

}  // namespace

}  // namespace policy
