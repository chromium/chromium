// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"

#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/mock_personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/variations/pref_names.h"                     // nogncheck
#include "components/variations/service/google_groups_manager.h"  // nogncheck
#include "components/variations/service/google_groups_manager_prefs.h"  // nogncheck
#include "components/variations/variations_seed_processor.h"  // nogncheck
#endif

namespace autofill {

namespace {

using ::testing::Return;

class AtMemoryEnablementUtilsTest : public testing::Test {
 protected:
  AtMemoryEnablementUtilsTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillAtMemory, {{"at_memory_eligible_tiers", ""}});
    // Enable the toggle by default in tests since it represents the default
    // active state.
    autofill_client().GetPrefs()->SetUserPref(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        base::Value(true));
    // Set PersonalContextService to return not eligible by default.
    ON_CALL(personal_context_service_, GetEnablementState)
        .WillByDefault(Return(personal_context::PersonalContextEnablementState::
                                  kDisabledNotEligible));
    autofill_client().set_personal_context_enablement_service(
        &personal_context_service_);
  }

  TestAutofillClient& autofill_client() { return autofill_client_; }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<personal_context::MockPersonalContextEnablementService>
      personal_context_service_;

 private:
  TestAutofillClient autofill_client_;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Tests that `MayPerformAtMemoryAction` returns false when AtMemory is
// disabled.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_AtMemoryDisabled) {
  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(features::kAutofillAtMemory);
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        autofill_client()));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                        autofill_client()));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut, autofill_client()));
}

// Tests that `MayPerformAtMemoryAction` returns false when
// `personal_context_service` is null.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_NullPersonalContextService) {
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, nullptr,
      autofill_client().GetSubscriptionEligibilityService(),
      autofill_client().GetPrefs(), nullptr));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kShowAtMemoryInSettings, nullptr,
      autofill_client().GetSubscriptionEligibilityService(),
      autofill_client().GetPrefs(), nullptr));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut, nullptr,
      autofill_client().GetSubscriptionEligibilityService(),
      autofill_client().GetPrefs(), nullptr));
}

// Tests that `MayPerformAtMemoryAction` returns false when
// `subscription_eligibility_service` is null.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_NullSubscriptionTierEligibilityService) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAtMemory, {{"at_memory_eligible_tiers", "1"}});

  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        &personal_context_service_, nullptr,
                                        autofill_client().GetPrefs(), nullptr));
}

// Tests `MayPerformAtMemoryAction` when `pref_service` is null.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_NullPrefService) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  // IsPersonalContextToggleOn returns false if pref_service is null.
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, &personal_context_service_,
      autofill_client().GetSubscriptionEligibilityService(), nullptr, nullptr));
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kShowAtMemoryInSettings, &personal_context_service_,
      autofill_client().GetSubscriptionEligibilityService(), nullptr, nullptr));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut,
      &personal_context_service_,
      autofill_client().GetSubscriptionEligibilityService(), nullptr, nullptr));
}

// Tests `MayPerformAtMemoryAction` under various Personal Context states.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_States) {
  autofill_client().GetPrefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));

  // State: kEnabled
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut, autofill_client()));

  // State: kEnabledShouldShowNotice
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(Return(personal_context::PersonalContextEnablementState::
                           kEnabledShouldShowNotice));
  autofill_client().GetPrefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut, autofill_client()));

  // State: kDisabledViaPersonalIntelligenceInAutofillToggle
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(Return(personal_context::PersonalContextEnablementState::
                           kDisabledViaPersonalIntelligenceInAutofillToggle));
  autofill_client().GetPrefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(false));
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                       autofill_client()));

  // State: kDisabledNeedsOptIn
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(Return(personal_context::PersonalContextEnablementState::
                           kDisabledNeedsOptIn));
  autofill_client().GetPrefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(false));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                        autofill_client()));

  // State: kDisabledNotEligible
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(Return(personal_context::PersonalContextEnablementState::
                           kDisabledNotEligible));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        autofill_client()));
}

// Tests `MayPerformAtMemoryAction` when the toggle pref is off.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_ToggleOff) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  autofill_client().GetPrefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(false));

  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        autofill_client()));
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                       autofill_client()));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut, autofill_client()));
}

// Tests that `MayPerformAtMemoryAction` returns false when
// `personal_context_service` returns `kDisabledNotEligible`.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_NotSupported) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(Return(personal_context::PersonalContextEnablementState::
                                 kDisabledNotEligible));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        autofill_client()));
}

// Tests that `MayPerformAtMemoryAction` returns true when the client supports
// AtMemory and the settings toggle is enabled.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_SupportedAndToggleOn) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  autofill_client().GetPrefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       autofill_client()));
}

// Tests that when `kAtMemorySkipEligibilityChecks` is enabled,
// `MayPerformAtMemoryAction` returns true even if the user is not eligible,
// provided that the base `kAutofillAtMemory` feature is enabled.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_SkipEligibilityChecks) {
  base::test::ScopedFeatureList debug_features(
      features::debug::kAtMemorySkipEligibilityChecks);

  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(Return(personal_context::PersonalContextEnablementState::
                                 kDisabledNotEligible));

  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       autofill_client()));
}

// Tests that a user is eligible for AtMemory if their subscription tier is in
// the list of eligible tiers configured by the feature parameters.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_SubscriptionTierEligibility) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAtMemory, {{"at_memory_eligible_tiers", "1,2"}});

  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       autofill_client()));

  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 2);
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       autofill_client()));

  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 3);
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        autofill_client()));
}

// Tests that if `at_memory_eligible_tiers` is empty, then the user is eligible
// regardless of their tier, and even if SubscriptionEligibilityService is null.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_SubscriptionTierEligibility_EmptyList) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAtMemory, {{"at_memory_eligible_tiers", ""}});

  // The user is eligible even if SubscriptionEligibilityService is null.
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       &personal_context_service_, nullptr,
                                       autofill_client().GetPrefs(), nullptr));

  // The user is eligible for any tier value.
  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 999);
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       autofill_client()));
}

// Tests that if `at_memory_eligible_tiers` is not defined, then the user is
// eligible.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_SubscriptionTierEligibility_NotDefined) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAutofillAtMemory);

  // The user is eligible even if SubscriptionEligibilityService is null.
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       &personal_context_service_, nullptr,
                                       autofill_client().GetPrefs(), nullptr));

  // The user is eligible for any tier value.
  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 999);
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       autofill_client()));
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Tests for non-branded Chromium builds.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Tests that `MayPerformAtMemoryAction` returns false for non-branded Chromium
// build even when all conditions are met.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_SupportedAndToggleOn) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  autofill_client().GetPrefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        autofill_client()));
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_FUCHSIA)
class AtMemoryEnablementUtilsWithGroupsTest
    : public AtMemoryEnablementUtilsTest {
 protected:
  AtMemoryEnablementUtilsWithGroupsTest() {
    local_state_.registry()->RegisterDictionaryPref(
        variations::prefs::kVariationsGoogleGroups);
    autofill_client().GetPrefs()->registry()->RegisterListPref(
        GetDogfoodGroupsPrefName());

    autofill_client().set_google_groups_manager(
        std::make_unique<GoogleGroupsManager>(local_state_, "DefaultKey",
                                              *autofill_client().GetPrefs()));
  }

  static constexpr std::string GetDogfoodGroupsPrefName() {
#if BUILDFLAG(IS_CHROMEOS)
    return variations::kOsDogfoodGroupsSyncPrefName;
#else
    return variations::kDogfoodGroupsSyncPrefName;
#endif
  }

  void SetUserGroups(const std::vector<std::string>& groups) {
    base::ListValue pref_groups_list;
    for (const std::string& group : groups) {
      base::DictValue group_dict;
      group_dict.Set(variations::kDogfoodGroupsSyncPrefGaiaIdKey, group);
      pref_groups_list.Append(std::move(group_dict));
    }
    autofill_client().GetPrefs()->SetUserPref(
        GetDogfoodGroupsPrefName(), base::Value(std::move(pref_groups_list)));
  }

 private:
  TestingPrefServiceSimple local_state_;
};

// Tests that the action is not allowed if a Google Group is required but the
// user is not a member of that group.
TEST_F(AtMemoryEnablementUtilsWithGroupsTest,
       MayPerformAtMemoryAction_GroupRequired_UserNotInGroup) {
  constexpr char kRequiredGroup[] = "at-memory-dogfooders";
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAtMemory,
      {{variations::internal::kGoogleGroupFeatureParamName, kRequiredGroup}});

  SetUserGroups({"some-other-group", "another-group"});
  ON_CALL(personal_context_service_, GetEnablementState)
      .WillByDefault(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        autofill_client()));
}

// Tests that the action is allowed if a Google Group is required and the user
// is a member of that group.
TEST_F(AtMemoryEnablementUtilsWithGroupsTest,
       MayPerformAtMemoryAction_GroupRequired_UserInGroup) {
  constexpr char kRequiredGroup[] = "at-memory-dogfooders";
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAtMemory,
      {{variations::internal::kGoogleGroupFeatureParamName, kRequiredGroup}});

  SetUserGroups({"some-other-group", kRequiredGroup});
  ON_CALL(personal_context_service_, GetEnablementState)
      .WillByDefault(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       autofill_client()));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_FUCHSIA)

}  // namespace
}  // namespace autofill
