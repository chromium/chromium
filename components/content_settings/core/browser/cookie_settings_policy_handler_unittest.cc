// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace content_settings {

class CookieSettingsPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
 public:
  void SetUp() override {
    handler_list_.AddHandler(std::make_unique<CookieSettingsPolicyHandler>());
  }

 protected:
  void SetThirdPartyCookiePolicy(bool third_party_cookie_blocking_enabled) {
    policy::PolicyMap policy;
    policy.Set(
        policy::key::kBlockThirdPartyCookies, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        std::make_unique<base::Value>(third_party_cookie_blocking_enabled),
        nullptr);
    UpdateProviderPolicy(policy);
  }
};

TEST_F(CookieSettingsPolicyHandlerTest, ThirdPartyCookieBlockingNotSet) {
  policy::PolicyMap policy;
  UpdateProviderPolicy(policy);
  const base::Value* value;
  EXPECT_FALSE(store_->GetValue(prefs::kCookieControlsMode, &value));
}

TEST_F(CookieSettingsPolicyHandlerTest, ThirdPartyCookieBlockingEnabled) {
  SetThirdPartyCookiePolicy(true);
  const base::Value* value;
  ASSERT_TRUE(store_->GetValue(prefs::kCookieControlsMode, &value));
  EXPECT_EQ(static_cast<CookieControlsMode>(value->GetInt()),
            CookieControlsMode::kOff);
}

TEST_F(CookieSettingsPolicyHandlerTest, ThirdPartyCookieBlockingDisabled) {
  SetThirdPartyCookiePolicy(false);
  const base::Value* value;
  ASSERT_TRUE(store_->GetValue(prefs::kCookieControlsMode, &value));
  EXPECT_EQ(static_cast<CookieControlsMode>(value->GetInt()),
            CookieControlsMode::kOff);
}

}  // namespace content_settings
