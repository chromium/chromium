// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/policy/find_and_fill_with_gemini_settings_policy_handler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/values.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

class FindAndFillWithGeminiSettingsPolicyHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
        gen_ai_default_policies;
    gen_ai_default_policies.emplace_back(
        key::kFindAndFillWithGeminiSettings,
        optimization_guide::prefs::kFindAndFillWithGeminiSettings);
    gen_ai_default_policies.emplace_back(
        key::kGeminiSettings, optimization_guide::prefs::kGeminiSettings,
        GenAiDefaultSettingsPolicyHandler::PolicyValueToPrefMap(
            {{0, 0}, {1, 0}, {2, 1}}));

    gen_ai_default_handler_ =
        std::make_unique<GenAiDefaultSettingsPolicyHandler>(
            std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>(
                gen_ai_default_policies));

    handler_ = std::make_unique<FindAndFillWithGeminiSettingsPolicyHandler>(
        std::make_unique<GenAiDefaultSettingsPolicyHandler>(
            std::move(gen_ai_default_policies)));
  }

  void SetPolicyValue(const char* policy_name, int value) {
    policies_.Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, base::Value(value), nullptr);
  }

  bool CheckPolicySettings() {
    return handler_->CheckPolicySettings(policies_, &errors_);
  }

  void ApplyPolicySettings() {
    gen_ai_default_handler_->ApplyPolicySettings(policies_, &prefs_);
    handler_->ApplyPolicySettings(policies_, &prefs_);
  }

  std::optional<int> GetPrefValue() const {
    int value = -1;
    if (prefs_.GetInteger(
            optimization_guide::prefs::kFindAndFillWithGeminiSettings,
            &value)) {
      return value;
    }
    return std::nullopt;
  }

  const PolicyErrorMap& errors() const { return errors_; }

 private:
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
  std::unique_ptr<GenAiDefaultSettingsPolicyHandler> gen_ai_default_handler_;
  std::unique_ptr<FindAndFillWithGeminiSettingsPolicyHandler> handler_;
};

// Tests that an out-of-range policy value returns false from
// CheckPolicySettings.
TEST_F(FindAndFillWithGeminiSettingsPolicyHandlerTest, InvalidPolicyValue) {
  SetPolicyValue(key::kFindAndFillWithGeminiSettings, 3);
  EXPECT_FALSE(CheckPolicySettings());
}

// Tests that when GeminiSettings is set to 1 (Disabled) and
// FindAndFillWithGeminiSettings = 0 (Allowed), CheckPolicySettings returns true
// (emitting a dependency error) and ApplyPolicySettings sets pref to 2
// (Disabled).
TEST_F(FindAndFillWithGeminiSettingsPolicyHandlerTest,
       GeminiDisabled_FindAndFillAllowed_EmitsErrorAndForcesPrefDisabled) {
  SetPolicyValue(key::kGeminiSettings, 1);
  SetPolicyValue(key::kFindAndFillWithGeminiSettings, 0);
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_FALSE(errors().empty());
  ApplyPolicySettings();
  EXPECT_EQ(GetPrefValue(), 2);
}

}  // namespace

}  // namespace policy
