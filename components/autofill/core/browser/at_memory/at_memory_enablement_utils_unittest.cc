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
#include "components/prefs/pref_notifier_impl.h"
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

// A helper PrefStore that allows us to count how many times a specific
// preference (the personal context toggle) is read. This is used to verify
// the evaluation order of checks in MayPerformAtMemoryAction.
class CountingPrefStore : public TestingPrefStore {
 public:
  explicit CountingPrefStore(std::string_view observed_key)
      : observed_key_(observed_key) {}

  bool GetValue(std::string_view key,
                const base::Value** result) const override {
    if (key == observed_key_) {
      ++call_count_;
    }
    return TestingPrefStore::GetValue(key, result);
  }

  int call_count() const { return call_count_; }

 private:
  ~CountingPrefStore() override = default;
  const std::string observed_key_;
  mutable int call_count_ = 0;
};

// A helper PrefService that allows us to inject our custom CountingPrefStore
// for user preferences. We must inherit from TestingPrefServiceBase because
// TestingPrefServiceSimple does not allow injecting a custom PrefStore.
class TestPrefService
    : public TestingPrefServiceBase<PrefService, PrefRegistry> {
 public:
  TestPrefService(scoped_refptr<TestingPrefStore> user_prefs,
                  scoped_refptr<PrefRegistry> pref_registry)
      : TestingPrefServiceBase<PrefService, PrefRegistry>(
            base::MakeRefCounted<TestingPrefStore>(),
            base::MakeRefCounted<TestingPrefStore>(),
            base::MakeRefCounted<TestingPrefStore>(),
            user_prefs,
            base::MakeRefCounted<TestingPrefStore>(),
            pref_registry,
            new PrefNotifierImpl()) {}
};

class AtMemoryEnablementUtilsTest : public testing::Test {
 protected:
  AtMemoryEnablementUtilsTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillAtMemory, {{"at_memory_eligible_tiers", ""}});
    autofill_client().GetPrefs()->registry()->RegisterIntegerPref(
        "browser.gemini_settings", 0);
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
    autofill_client().set_last_committed_primary_main_frame_url(
        GURL("https://example.com"));
  }

  TestAutofillClient& autofill_client() { return autofill_client_; }
  const GURL& form_url() const { return form_url_; }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<personal_context::MockPersonalContextEnablementService>
      personal_context_service_;

 private:
  TestAutofillClient autofill_client_;
  const GURL form_url_{"https://example.com/form"};
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

  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                        autofill_client()));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut, autofill_client()));
}

// Tests that `MayPerformAtMemoryAction` returns false when the Gemini settings
// enterprise policy disables Gemini.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_GeminiPolicyDisabled) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  // Value 1 means not available.
  // components/policy/resources/templates/policy_definitions/GenerativeAI/GeminiSettings.yaml
  autofill_client().GetPrefs()->SetInteger("browser.gemini_settings", 1);

  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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
      autofill_client().GetPrefs(), nullptr, nullptr,
      GURL("https://example.com")));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kShowAtMemoryInSettings, nullptr,
      autofill_client().GetSubscriptionEligibilityService(),
      autofill_client().GetPrefs(), nullptr, nullptr));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut, nullptr,
      autofill_client().GetSubscriptionEligibilityService(),
      autofill_client().GetPrefs(), nullptr, nullptr));
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
                                        autofill_client().GetPrefs(), nullptr,
                                        nullptr, GURL("https://example.com")));
}

// Tests `MayPerformAtMemoryAction` when `pref_service` is null.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_NullPrefService) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  // IsPersonalContextToggleOn returns false if pref_service is null.
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, &personal_context_service_,
      autofill_client().GetSubscriptionEligibilityService(), nullptr, nullptr,
      nullptr, GURL("https://example.com")));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kShowAtMemoryInSettings, &personal_context_service_,
      autofill_client().GetSubscriptionEligibilityService(), nullptr, nullptr,
      nullptr));
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kAllowCustomizeAtMemoryShortcut,
      &personal_context_service_,
      autofill_client().GetSubscriptionEligibilityService(), nullptr, nullptr,
      nullptr));
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
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
}

// Tests `MayPerformAtMemoryAction` when the toggle pref is off.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_ToggleOff) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  autofill_client().GetPrefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(false));

  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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

  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));

  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 2);
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));

  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 3);
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, &personal_context_service_, nullptr,
      autofill_client().GetPrefs(), nullptr,
      autofill_client().GetAutofillOptimizationGuideDecider(),
      GURL("https://example.com")));

  // The user is eligible for any tier value.
  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 999);
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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

  // The user is eligible even if `SubscriptionEligibilityService` is null.
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, &personal_context_service_, nullptr,
      autofill_client().GetPrefs(), nullptr,
      autofill_client().GetAutofillOptimizationGuideDecider(),
      GURL("https://example.com")));

  // The user is eligible for any tier value.
  autofill_client().GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 999);
  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
}

// Tests that `MayPerformAtMemoryAction` returns false when the domain is
// blocklisted by the optimization guide.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_BlocklistedByOptimizationGuide) {
  ON_CALL(personal_context_service_, GetEnablementState)
      .WillByDefault(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  // Define URLs
  GURL allowed_main("https://allowed.com");
  GURL allowed_form("https://allowed.com/form");
  GURL blocked_main("https://blocked-main.com");
  GURL blocked_form("https://blocked-form.com/form");

  // Mock Decider
  auto* decider = autofill_client().GetAutofillOptimizationGuideDecider();
  ON_CALL(*decider, ShouldBlockAtMemory(allowed_main))
      .WillByDefault(Return(false));
  ON_CALL(*decider, ShouldBlockAtMemory(allowed_form))
      .WillByDefault(Return(false));
  ON_CALL(*decider, ShouldBlockAtMemory(blocked_main))
      .WillByDefault(Return(true));
  ON_CALL(*decider, ShouldBlockAtMemory(blocked_form))
      .WillByDefault(Return(true));

  // 1. Allowed -> Should return true
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       autofill_client(), allowed_main));

  // 2. Blocked -> Should return false
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        autofill_client(), blocked_main));
}

// Tests that when the feature is disabled, the toggle pref is still checked
// if the user is eligible but the toggle is OFF. This verifies that the
// toggle check occurs before the feature flag check.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_FeatureCheckedLast_ToggleOff) {
  auto pref_store = base::MakeRefCounted<CountingPrefStore>(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus);
  auto registry = base::MakeRefCounted<PrefRegistrySimple>();
  registry->RegisterBooleanPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);
  registry->RegisterIntegerPref("browser.gemini_settings", 0);
  auto pref_service = std::make_unique<TestPrefService>(pref_store, registry);

  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(features::kAutofillAtMemory);

  ON_CALL(personal_context_service_, GetEnablementState)
      .WillByDefault(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  pref_store->SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);

  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, &personal_context_service_,
      /*subscription_eligibility_service=*/nullptr, pref_service.get(),
      /*google_groups_manager=*/nullptr,
      autofill_client().GetAutofillOptimizationGuideDecider(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
  EXPECT_EQ(pref_store->call_count(), 1);
}

// Tests that when the feature is disabled, the toggle pref is checked
// if the user is eligible and the toggle is ON.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_FeatureCheckedLast_ToggleOn) {
  auto pref_store = base::MakeRefCounted<CountingPrefStore>(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus);
  auto registry = base::MakeRefCounted<PrefRegistrySimple>();
  registry->RegisterBooleanPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);
  registry->RegisterIntegerPref("browser.gemini_settings", 0);
  auto pref_service = std::make_unique<TestPrefService>(pref_store, registry);

  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(features::kAutofillAtMemory);

  ON_CALL(personal_context_service_, GetEnablementState)
      .WillByDefault(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  pref_store->SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);

  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, &personal_context_service_,
      /*subscription_eligibility_service=*/nullptr, pref_service.get(),
      /*google_groups_manager=*/nullptr,
      autofill_client().GetAutofillOptimizationGuideDecider(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
  EXPECT_EQ(pref_store->call_count(), 1);
}

// Tests that if the user is NOT eligible, the function returns early
// without checking the toggle pref, verifying that eligibility is checked
// before the toggle.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_FeatureCheckedLast_NotEligible) {
  auto pref_store = base::MakeRefCounted<CountingPrefStore>(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus);
  auto registry = base::MakeRefCounted<PrefRegistrySimple>();
  registry->RegisterBooleanPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);
  registry->RegisterIntegerPref("browser.gemini_settings", 0);
  auto pref_service = std::make_unique<TestPrefService>(pref_store, registry);

  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(features::kAutofillAtMemory);

  ON_CALL(personal_context_service_, GetEnablementState)
      .WillByDefault(Return(personal_context::PersonalContextEnablementState::
                                kDisabledNotEligible));
  pref_store->SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);

  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, &personal_context_service_,
      /*subscription_eligibility_service=*/nullptr, pref_service.get(),
      /*google_groups_manager=*/nullptr,
      autofill_client().GetAutofillOptimizationGuideDecider(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
  EXPECT_EQ(pref_store->call_count(), 0);
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
  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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

  EXPECT_FALSE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
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

  EXPECT_TRUE(MayPerformAtMemoryAction(
      AtMemoryAction::kTriggerSearchUI, autofill_client(),
      autofill_client().GetLastCommittedPrimaryMainFrameURL()));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_FUCHSIA)

}  // namespace
}  // namespace autofill
