// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/switches.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

class MockFilterStore : public FilterStore {
 public:
  MockFilterStore() = default;
  ~MockFilterStore() override = default;

  MOCK_METHOD(void,
              DeleteAnnotationsForHosts,
              (std::vector<std::string> hosts,
               base::Time delete_begin,
               base::Time delete_end,
               base::OnceCallback<void(std::optional<int64_t>)> callback),
              (override));
};

class MultistepFilterServiceTest : public testing::Test {
 public:
  MultistepFilterServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(kMultistepFilter);
    pref_service_.registry()->RegisterBooleanPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
    RegisterRetentionProfilePrefs(pref_service_.registry());
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        pref_service_.registry());
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_.registry());
    sync_service_.GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, true);
  }

  void CreateService(std::unique_ptr<FilterStore> filter_store =
                         std::make_unique<FilterStore>()) {
    auto annotation_index_client =
        std::make_unique<MockAnnotationIndexClient>();
    mock_client_ = annotation_index_client.get();
    auto consent_helper = unified_consent::UrlKeyedDataCollectionConsentHelper::
        NewAnonymizedDataCollectionConsentHelper(&pref_service_);

    MultistepFilterService::Params params;
    params.annotation_index_client = std::move(annotation_index_client);
    params.filter_store = std::move(filter_store);
    params.identity_manager = identity_test_env_.identity_manager();
    params.consent_helper = std::move(consent_helper);
    params.log_router = nullptr;
    params.pref_service = &pref_service_;
    params.sync_service = &sync_service_;

    service_ = std::make_unique<MultistepFilterService>(std::move(params));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;

  std::unique_ptr<MultistepFilterService> service_;

  // Raw pointers to the mocks, valid as long as the service is alive.
  raw_ptr<MockAnnotationIndexClient> mock_client_ = nullptr;
};

// Tests that the service can be created and destroyed without crashing.
TEST_F(MultistepFilterServiceTest, CreateAndDestroy) {
  // Verifies the service can be created and destroyed without crashing.
  CreateService();
}

// Tests that the service can be destroyed without crashing when the history
// service is null.
TEST_F(MultistepFilterServiceTest,
       OnHistoryDeletions_InvalidTimeRangeDoesNotCrash) {
  CreateService();
  history::DeletionInfo deletion_info = history::DeletionInfo::ForUrls(
      {history::URLRow(GURL("https://example.com"))},
      /*favicon_urls=*/{});

  // Call OnHistoryDeletions. Since the time_range is invalid, it historically
  // crashed. With the fix, it should succeed without crashing.
  service_->OnHistoryDeletions(/*history_service=*/nullptr, deletion_info);
}

// Tests that the service correctly records suggestion outcomes in the
// preferences.
TEST_F(MultistepFilterServiceTest, RecordSuggestionOutcomesUpdatesPrefs) {
  CreateService();
  RetentionStateSnapshot initial_state = GetRetentionState(&pref_service_);
  EXPECT_EQ(initial_state.suggestion_impressions, 0);
  EXPECT_EQ(initial_state.suggestion_acceptances, 0);
  EXPECT_FALSE(initial_state.is_last_suggestion_accepted);

  service_->RecordSuggestionImpression();
  RetentionStateSnapshot impression_state = GetRetentionState(&pref_service_);
  EXPECT_EQ(impression_state.suggestion_impressions, 1);
  EXPECT_EQ(impression_state.suggestion_acceptances, 0);
  EXPECT_FALSE(impression_state.is_last_suggestion_accepted);

  service_->RecordUserInteractionWithSuggestion(
      SuggestionUserDecision::kAccepted);
  RetentionStateSnapshot acceptance_state = GetRetentionState(&pref_service_);
  EXPECT_EQ(acceptance_state.suggestion_impressions, 1);
  EXPECT_EQ(acceptance_state.suggestion_acceptances, 1);
  EXPECT_TRUE(acceptance_state.is_last_suggestion_accepted);
}

// Tests that the service correctly returns the current retention state.
TEST_F(MultistepFilterServiceTest, GetRetentionStateReturnsCorrectSnapshot) {
  CreateService();

  // Verify initial state.
  RetentionStateSnapshot initial_state = service_->GetRetentionState();
  EXPECT_EQ(initial_state.suggestion_impressions, 0);
  EXPECT_EQ(initial_state.suggestion_acceptances, 0);
  EXPECT_FALSE(initial_state.is_last_suggestion_accepted);

  // Modify state.
  service_->RecordSuggestionImpression();
  service_->RecordUserInteractionWithSuggestion(
      SuggestionUserDecision::kAccepted);

  // Verify updated state is returned by GetRetentionState.
  RetentionStateSnapshot updated_state = service_->GetRetentionState();
  EXPECT_EQ(updated_state.suggestion_impressions, 1);
  EXPECT_EQ(updated_state.suggestion_acceptances, 1);
  EXPECT_TRUE(updated_state.is_last_suggestion_accepted);
}

// Tests that the service correctly returns the account state when the user is
// not signed in.
TEST_F(MultistepFilterServiceTest, GetAccountState_NotSignedIn) {
  // 1. Default state (Not signed in)
  CreateService();
  {
    AccountState account = service_->GetAccountState();
    EXPECT_FALSE(account.is_signed_in);
    EXPECT_FALSE(account.can_use_model_execution_features);
    EXPECT_FALSE(account.IsEligible());
  }

  // 2. Not signed in, bypass switch enabled
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kMultistepFilterBypassCapabilityCheck);
    CreateService();
    AccountState account = service_->GetAccountState();
    EXPECT_FALSE(account.is_signed_in);
    EXPECT_TRUE(account.can_use_model_execution_features);
    EXPECT_FALSE(account.IsEligible());
  }
}

// Tests that the service correctly returns the account state when the user is
// signed in.
TEST_F(MultistepFilterServiceTest, GetAccountState_SignedIn) {
  // 1. Signed in, capabilities not set
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  CreateService();
  {
    AccountState account = service_->GetAccountState();
    EXPECT_TRUE(account.is_signed_in);
    EXPECT_FALSE(account.can_use_model_execution_features);
    EXPECT_FALSE(account.IsEligible());
  }

  // 2. Signed in, capabilities explicitly false
  AccountInfo account_info =
      identity_test_env_.identity_manager()
          ->FindExtendedAccountInfoByEmailAddress("test@gmail.com");
  {
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(false);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
  }
  CreateService();
  {
    AccountState account = service_->GetAccountState();
    EXPECT_TRUE(account.is_signed_in);
    EXPECT_FALSE(account.can_use_model_execution_features);
    EXPECT_FALSE(account.IsEligible());
  }

  // 3. Signed in, capabilities enabled
  {
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
  }
  CreateService();
  {
    AccountState account = service_->GetAccountState();
    EXPECT_TRUE(account.is_signed_in);
    EXPECT_TRUE(account.can_use_model_execution_features);
    EXPECT_TRUE(account.IsEligible());
  }

  // 4. Signed in, capabilities disabled, bypass switch enabled
  {
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(false);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);

    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kMultistepFilterBypassCapabilityCheck);
    CreateService();
    AccountState account = service_->GetAccountState();
    EXPECT_TRUE(account.is_signed_in);
    EXPECT_TRUE(account.can_use_model_execution_features);
    EXPECT_TRUE(account.IsEligible());
  }
}

// Tests that the service correctly returns the consent state in different
// scenarios.
TEST_F(MultistepFilterServiceTest, GetConsentState) {
  // 1. MSBB disabled, sync disabled
  pref_service_.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {});
  CreateService();
  {
    ConsentState consent = service_->GetConsentState();
    EXPECT_FALSE(consent.is_msbb_enabled);
    EXPECT_FALSE(consent.is_history_sync_enabled);
    EXPECT_FALSE(consent.IsFullyConsented());
  }

  // 2. MSBB disabled, sync enabled
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});
  CreateService();
  {
    ConsentState consent = service_->GetConsentState();
    EXPECT_FALSE(consent.is_msbb_enabled);
    EXPECT_TRUE(consent.is_history_sync_enabled);
    EXPECT_FALSE(consent.IsFullyConsented());
  }

  // 3. MSBB enabled, sync disabled
  pref_service_.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {});
  CreateService();
  {
    ConsentState consent = service_->GetConsentState();
    EXPECT_TRUE(consent.is_msbb_enabled);
    EXPECT_FALSE(consent.is_history_sync_enabled);
    EXPECT_FALSE(consent.IsFullyConsented());
  }

  // 4. MSBB enabled, sync enabled
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});
  CreateService();
  {
    ConsentState consent = service_->GetConsentState();
    EXPECT_TRUE(consent.is_msbb_enabled);
    EXPECT_TRUE(consent.is_history_sync_enabled);
    EXPECT_TRUE(consent.IsFullyConsented());
  }
}

// Tests that OnHistoryDeletions does not call DeleteAnnotationsForHosts when
// there are no deletions.
TEST_F(MultistepFilterServiceTest, OnHistoryDeletions_NoOpDeletionsIgnored) {
  auto mock_store = std::make_unique<testing::NiceMock<MockFilterStore>>();
  EXPECT_CALL(*mock_store, DeleteAnnotationsForHosts).Times(0);
  CreateService(std::move(mock_store));
  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(/*deleted_rows=*/{},
                                     /*favicon_urls=*/{});
  service_->OnHistoryDeletions(/*history_service=*/nullptr, deletion_info);
}

// Tests that the service correctly returns the settings state in different
// scenarios.
TEST_F(MultistepFilterServiceTest, GetSettingsState) {
  using optimization_guide::prefs::FeatureOptInState;

  // 1. Default state (Prefs not explicitly modified)
  CreateService();
  {
    SettingsState settings = service_->GetSettingsState();
    EXPECT_EQ(settings.opt_in_state, FeatureOptInState::kNotInitialized);
    EXPECT_EQ(settings.policy_state, SuggestionsPolicyState::kEnabled);
    EXPECT_TRUE(settings.IsSmartSuggestionsEnabled());
  }

  // 2. User opt-in enabled, policy enabled
  pref_service_.SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing),
      static_cast<int>(FeatureOptInState::kEnabled));
  CreateService();
  {
    SettingsState settings = service_->GetSettingsState();
    EXPECT_EQ(settings.opt_in_state, FeatureOptInState::kEnabled);
    EXPECT_EQ(settings.policy_state, SuggestionsPolicyState::kEnabled);
    EXPECT_TRUE(settings.IsSmartSuggestionsEnabled());
  }

  // 3. User opt-in disabled, policy enabled
  pref_service_.SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing),
      static_cast<int>(FeatureOptInState::kDisabled));
  CreateService();
  {
    SettingsState settings = service_->GetSettingsState();
    EXPECT_EQ(settings.opt_in_state, FeatureOptInState::kDisabled);
    EXPECT_EQ(settings.policy_state, SuggestionsPolicyState::kEnabled);
    EXPECT_FALSE(settings.IsSmartSuggestionsEnabled());
  }

  // 4. User opt-in enabled, policy disabled
  pref_service_.SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing),
      static_cast<int>(FeatureOptInState::kEnabled));
  pref_service_.SetInteger(
      optimization_guide::prefs::kChromeSuggestionsSettings,
      std::to_underlying(SuggestionsPolicyState::kDisabled));
  CreateService();
  {
    SettingsState settings = service_->GetSettingsState();
    EXPECT_EQ(settings.opt_in_state, FeatureOptInState::kEnabled);
    EXPECT_EQ(settings.policy_state, SuggestionsPolicyState::kDisabled);
    EXPECT_FALSE(settings.IsSmartSuggestionsEnabled());
  }

  // 5. User opt-in enabled, policy unknown (falls back to enabled)
  pref_service_.SetInteger(
      optimization_guide::prefs::kChromeSuggestionsSettings,
      /*unknown value=*/99);
  CreateService();
  {
    SettingsState settings = service_->GetSettingsState();
    EXPECT_EQ(settings.opt_in_state, FeatureOptInState::kEnabled);
    EXPECT_EQ(settings.policy_state, SuggestionsPolicyState::kEnabled);
    EXPECT_TRUE(settings.IsSmartSuggestionsEnabled());
  }
}

}  // namespace multistep_filter
