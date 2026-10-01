// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/policy/autofill_gen_ai_settings_policy_handler.h"

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

class AutofillGenAiSettingsPolicyHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
        gen_ai_default_policies;
    gen_ai_default_policies.emplace_back(
        key::kGeminiSettings, optimization_guide::prefs::kGeminiSettings,
        GenAiDefaultSettingsPolicyHandler::PolicyValueToPrefMap(
            {{0, 0}, {1, 0}, {2, 1}}));
    handler_ = std::make_unique<AutofillGenAiSettingsPolicyHandler>(
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
    handler_->ApplyPolicySettings(policies_, &prefs_);
  }

  // Pref gating AtMemory and Ambient Autofill.
  std::optional<int> GetFindAndFillPrefValue() const {
    return GetPrefValue(
        optimization_guide::prefs::kFindAndFillWithGeminiSettings);
  }

  // Pref gating "Smarter form understanding".
  std::optional<int> GetFormsAiPrefValue() const {
    return GetPrefValue(
        optimization_guide::prefs::
            kAutofillPredictionImprovementsEnterprisePolicyAllowed);
  }

  const PolicyErrorMap& errors() const { return errors_; }

 private:
  std::optional<int> GetPrefValue(const char* pref_name) const {
    int value = -1;
    if (prefs_.GetInteger(pref_name, &value)) {
      return value;
    }
    return std::nullopt;
  }

  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
  std::unique_ptr<AutofillGenAiSettingsPolicyHandler> handler_;
};

// Tests that an out-of-range policy value returns false from
// CheckPolicySettings.
TEST_F(AutofillGenAiSettingsPolicyHandlerTest, InvalidPolicyValue) {
  SetPolicyValue(key::kAutofillGenAiSettings, 3);
  EXPECT_FALSE(CheckPolicySettings());
}

// Tests that when GeminiSettings is set to 1 (Disabled) and
// AutofillGenAiSettings = 0 (Allowed), CheckPolicySettings succeeds without
// emitting a dependency error (since "Smarter form understanding" can still be
// enabled) and ApplyPolicySettings sets the AtMemory/Ambient pref to 2
// (Disabled) while keeping "Smarter form understanding" at 0 (Allowed).
TEST_F(AutofillGenAiSettingsPolicyHandlerTest,
       GeminiDisabled_ForcesFindAndFillDisabled_ButNotFormsAi) {
  SetPolicyValue(key::kGeminiSettings, 1);
  SetPolicyValue(key::kAutofillGenAiSettings, 0);
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_TRUE(errors().empty());
  ApplyPolicySettings();
  EXPECT_EQ(GetFindAndFillPrefValue(), 2);
  EXPECT_EQ(GetFormsAiPrefValue(), 0);
}

// Tests that disabling the Gemini app disables AtMemory and Ambient Autofill
// even when AutofillGenAiSettings is unset, and that the "Smarter form
// understanding" pref is left untouched so that the legacy
// AutofillPredictionSettings policy and GenAiDefaultSettings still apply.
TEST_F(AutofillGenAiSettingsPolicyHandlerTest,
       PolicyUnset_GeminiDisabled_ForcesFindAndFillDisabledOnly) {
  SetPolicyValue(key::kGeminiSettings, 1);
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_TRUE(errors().empty());
  ApplyPolicySettings();
  EXPECT_EQ(GetFindAndFillPrefValue(), 2);
  EXPECT_EQ(GetFormsAiPrefValue(), std::nullopt);
}

// Tests that when the policy is unset and the Gemini app is enabled, neither
// pref is written by this handler.
TEST_F(AutofillGenAiSettingsPolicyHandlerTest, PolicyUnset_GeminiEnabled) {
  SetPolicyValue(key::kGeminiSettings, 0);
  EXPECT_TRUE(CheckPolicySettings());
  ApplyPolicySettings();
  EXPECT_EQ(GetFindAndFillPrefValue(), std::nullopt);
  EXPECT_EQ(GetFormsAiPrefValue(), std::nullopt);
}

}  // namespace

}  // namespace policy
