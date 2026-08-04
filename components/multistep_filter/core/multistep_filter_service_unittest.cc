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

TEST_F(MultistepFilterServiceTest, CreateAndDestroy) {
  // Verifies the service can be created and destroyed without crashing.
  CreateService();
}



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

// Tests that when URL-keyed anonymized data collection (MSBB) is explicitly
// disabled in preferences, consent evaluates to false.
TEST_F(MultistepFilterServiceTest, HasUserProvidedConsent_MsbbDisabled) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});
  pref_service_.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  CreateService();
  EXPECT_FALSE(service_->HasUserProvidedConsent(1, "example.com"));
}

// Tests that when Chrome History Sync is explicitly disabled by the user,
// consent evaluates to false.
TEST_F(MultistepFilterServiceTest, HasUserProvidedConsent_HistorySyncDisabled) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  pref_service_.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {});

  CreateService();
  EXPECT_FALSE(service_->HasUserProvidedConsent(2, "example.com"));
}

// Tests that when the user is not signed in to a Chrome account, consent
// evaluates to false.
TEST_F(MultistepFilterServiceTest, HasUserProvidedConsent_NotSignedIn) {
  pref_service_.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});

  CreateService();
  EXPECT_FALSE(service_->HasUserProvidedConsent(3, "example.com"));
}

// Tests that when the user is signed in, has MSBB enabled, and has History Sync
// enabled, consent evaluates to true.
TEST_F(MultistepFilterServiceTest, HasUserProvidedConsent_FullyConsented) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  pref_service_.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});

  CreateService();
  EXPECT_TRUE(service_->HasUserProvidedConsent(4, "example.com"));
}

// Tests that CanUseModelExecutionFeatures returns false when not signed in.
TEST_F(MultistepFilterServiceTest, CanUseModelExecutionFeatures_NotSignedIn) {
  CreateService();
  EXPECT_FALSE(service_->CanUseModelExecutionFeatures());
}

// Tests that CanUseModelExecutionFeatures returns false when signed in but the
// capability is false.
TEST_F(MultistepFilterServiceTest,
       CanUseModelExecutionFeatures_CapabilityFalse) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                      account_info);

  CreateService();
  EXPECT_FALSE(service_->CanUseModelExecutionFeatures());
}

// Tests that CanUseModelExecutionFeatures returns true when signed in and the
// capability is true.
TEST_F(MultistepFilterServiceTest,
       CanUseModelExecutionFeatures_CapabilityTrue) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                      account_info);

  CreateService();
  EXPECT_TRUE(service_->CanUseModelExecutionFeatures());
}

// Tests that CanUseModelExecutionFeatures returns false when signed in and the
// capability is not explicitly set (defaults to kUnknown).
TEST_F(MultistepFilterServiceTest,
       CanUseModelExecutionFeatures_CapabilityNotSet) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  CreateService();
  EXPECT_FALSE(service_->CanUseModelExecutionFeatures());
}

// Tests that CanUseModelExecutionFeatures returns true when the bypass switch
// is enabled, even if not signed in.
TEST_F(MultistepFilterServiceTest,
       CanUseModelExecutionFeatures_BypassSwitchEnabled) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kMultistepFilterBypassCapabilityCheck);
  CreateService();
  EXPECT_TRUE(service_->CanUseModelExecutionFeatures());
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

// Tests that IsSmartSuggestionsEnabled returns true by default (when prefs are
// default/enabled).
TEST_F(MultistepFilterServiceTest, IsSmartSuggestionsEnabled_DefaultEnabled) {
  CreateService();
  EXPECT_TRUE(service_->IsSmartSuggestionsEnabled());
}

// Tests that IsSmartSuggestionsEnabled returns false when the user-controlled
// contextual cueing pref is disabled.
TEST_F(MultistepFilterServiceTest,
       IsSmartSuggestionsEnabled_OptInPrefDisabled) {
  pref_service_.SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing),
      static_cast<int>(
          optimization_guide::prefs::FeatureOptInState::kDisabled));
  CreateService();
  EXPECT_FALSE(service_->IsSmartSuggestionsEnabled());
}

// Tests that IsSmartSuggestionsEnabled returns false when the enterprise policy
// ChromeSuggestionsSettings is set to disabled.
TEST_F(MultistepFilterServiceTest,
       IsSmartSuggestionsEnabled_EnterprisePolicyDisabled) {
  pref_service_.SetInteger(
      optimization_guide::prefs::kChromeSuggestionsSettings,
      MultistepFilterService::kChromeSuggestionsSettingsDisabled);
  CreateService();
  EXPECT_FALSE(service_->IsSmartSuggestionsEnabled());
}

}  // namespace multistep_filter
