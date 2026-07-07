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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service_test_api.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

using ::testing::_;

constexpr int64_t kTestNavigationId = 12345;

class MockFilterExtractor : public FilterExtractor {
 public:
  MockFilterExtractor(AnnotationIndexClient& annotation_index_client,
                      FilterStore& filter_store)
      : FilterExtractor(annotation_index_client,
                        filter_store,
                        /*log_router=*/nullptr) {}
  MOCK_METHOD(
      void,
      ExtractAnnotationFromUrl,
      (const GURL& url,
       base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
       int64_t navigation_id),
      (override));
};

class MockFilterSuggestionGenerator : public FilterSuggestionGenerator {
 public:
  MockFilterSuggestionGenerator(AnnotationIndexClient& annotation_index_client,
                                FilterStore& filter_store)
      : FilterSuggestionGenerator(annotation_index_client,
                                  filter_store,
                                  /*log_router=*/nullptr) {}
  MOCK_METHOD(
      void,
      GenerateSuggestion,
      (const GURL& url,
       std::vector<std::string> supported_task_types,
       base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
       int64_t navigation_id),
      (override));
};

class MockObserver : public MultistepFilterService::ObserverForTest {
 public:
  MOCK_METHOD(void,
              OnExtractionFinished,
              (std::optional<base::Uuid>),
              (override));
  MOCK_METHOD(void,
              OnSuggestionGenerated,
              (std::optional<UrlFilterSuggestion>),
              (override));
};

class MultistepFilterServiceTest : public testing::Test {
 public:
  MultistepFilterServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(kMultistepFilter);
    pref_service_.registry()->RegisterBooleanPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
    RegisterRetentionProfilePrefs(pref_service_.registry());
    sync_service_.GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, true);
  }

  void CreateService(signin::IdentityManager* identity_manager) {
    auto annotation_index_client =
        std::make_unique<MockAnnotationIndexClient>();
    mock_client_ = annotation_index_client.get();
    auto filter_store = std::make_unique<FilterStore>();
    auto filter_extractor = std::make_unique<MockFilterExtractor>(
        *annotation_index_client, *filter_store);
    auto filter_suggestion_generator =
        std::make_unique<MockFilterSuggestionGenerator>(
            *annotation_index_client, *filter_store);

    mock_extractor_ = filter_extractor.get();
    mock_generator_ = filter_suggestion_generator.get();
    auto consent_helper = unified_consent::UrlKeyedDataCollectionConsentHelper::
        NewAnonymizedDataCollectionConsentHelper(&pref_service_);

    MultistepFilterService::Params params;
    params.annotation_index_client = std::move(annotation_index_client);
    params.filter_store = std::move(filter_store);
    params.identity_manager = identity_manager;
    params.consent_helper = std::move(consent_helper);
    params.log_router = nullptr;
    params.pref_service = &pref_service_;
    params.sync_service = &sync_service_;

    service_ = std::make_unique<MultistepFilterService>(std::move(params));

    MultistepFilterServiceTestApi(*service_).set_filter_extractor(
        std::move(filter_extractor));
    MultistepFilterServiceTestApi(*service_).set_filter_suggestion_generator(
        std::move(filter_suggestion_generator));

    mock_observer_ = std::make_unique<MockObserver>();
    MultistepFilterServiceTestApi(*service_).SetObserverForTest(
        mock_observer_.get());
  }

  void CreateService() { CreateService(identity_test_env_.identity_manager()); }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;

  std::unique_ptr<MockObserver> mock_observer_;
  std::unique_ptr<MultistepFilterService> service_;

  // Raw pointers to the mocks, valid as long as the service is alive.
  raw_ptr<MockAnnotationIndexClient> mock_client_ = nullptr;
  raw_ptr<MockFilterExtractor> mock_extractor_ = nullptr;
  raw_ptr<MockFilterSuggestionGenerator> mock_generator_ = nullptr;
};

TEST_F(MultistepFilterServiceTest, CreateAndDestroy) {
  // Verifies the service can be created and destroyed without crashing.
  CreateService();
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_HistorySyncDisabled) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);

  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)))
      .Times(1);

  service_->ExtractAnnotation(0, kUrl, std::nullopt);
}

TEST_F(MultistepFilterServiceTest,
       GenerateFilterSuggestions_HistorySyncDisabled) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);

  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);
  EXPECT_CALL(*mock_observer_, OnSuggestionGenerated(testing::Eq(std::nullopt)))
      .Times(1);

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  service_->GenerateFilterSuggestions(0, kUrl, future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");
  base::Uuid mock_uuid = base::Uuid::GenerateRandomV4();
  FilterAnnotation mock_annotation(mock_uuid, "task1", "example.com",
                                   base::Time::Now(), {});
  EXPECT_CALL(*mock_client_, GetSupportedTasks(kUrl, _, kTestNavigationId))
      .WillOnce(
          base::test::RunOnceCallback<1>(std::vector<std::string>{"task1"}));

  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(kUrl, _, kTestNavigationId))
      .WillOnce(base::test::RunOnceCallback<1>(mock_annotation));

  EXPECT_CALL(*mock_observer_,
              OnExtractionFinished(testing::Optional(mock_uuid)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl, std::nullopt);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_NotSignedIn) {
  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl, std::nullopt);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_NullIdentityManager) {
  CreateService(nullptr);
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl, std::nullopt);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_NotAllowedDomain) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "example.com"}});

  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://notexample.com");

  EXPECT_CALL(*mock_client_, GetSupportedTasks(kUrl, _, kTestNavigationId))
      .WillOnce(base::test::RunOnceCallback<1>(std::vector<std::string>()));

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl, std::nullopt);
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");
  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      mock_callback;
  UrlFilterSuggestion mock_suggestion(UrlFilterSuggestion::Params{
      .navigation_url = kUrl,
      .source_host = u"example.com",
      .extraction_timestamp = base::Time::Now(),
      .attribute_ui_labels = {},
      .triggering_navigation_id = kTestNavigationId,
      .triggering_host = "example.com",
      .task_type = "task1"});

  EXPECT_CALL(*mock_client_, GetSupportedTasks(kUrl, _, kTestNavigationId))
      .WillOnce(
          base::test::RunOnceCallback<1>(std::vector<std::string>{"task1"}));

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(kUrl, std::vector<std::string>{"task1"}, _,
                                 kTestNavigationId))
      .WillOnce(base::test::RunOnceCallback<2>(mock_suggestion));

  EXPECT_CALL(*mock_observer_,
              OnSuggestionGenerated(testing::Optional(mock_suggestion)));
  EXPECT_CALL(mock_callback, Run(testing::Optional(mock_suggestion)));

  service_->GenerateFilterSuggestions(kTestNavigationId, kUrl,
                                      mock_callback.Get());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_NotSignedIn) {
  CreateService();
  const GURL kUrl("http://example.com");
  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      mock_callback;

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);
  EXPECT_CALL(*mock_observer_,
              OnSuggestionGenerated(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_callback, Run(testing::Eq(std::nullopt)));

  service_->GenerateFilterSuggestions(kTestNavigationId, kUrl,
                                      mock_callback.Get());
}

TEST_F(MultistepFilterServiceTest,
       GenerateFilterSuggestions_NullIdentityManager) {
  CreateService(nullptr);
  const GURL kUrl("http://example.com");
  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      mock_callback;

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);
  EXPECT_CALL(*mock_observer_,
              OnSuggestionGenerated(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_callback, Run(testing::Eq(std::nullopt)));

  service_->GenerateFilterSuggestions(kTestNavigationId, kUrl,
                                      mock_callback.Get());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_NotAllowedDomain) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "example.com"}});

  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://notexample.com");
  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      mock_callback;

  EXPECT_CALL(*mock_client_, GetSupportedTasks(kUrl, _, kTestNavigationId))
      .WillOnce(base::test::RunOnceCallback<1>(std::vector<std::string>()));

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);
  EXPECT_CALL(*mock_observer_,
              OnSuggestionGenerated(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_callback, Run(testing::Eq(std::nullopt)));

  service_->GenerateFilterSuggestions(kTestNavigationId, kUrl,
                                      mock_callback.Get());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_NullCallback) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);

  service_->GenerateFilterSuggestions(kTestNavigationId, kUrl,
                                      base::NullCallback());
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_MsbbDisabled) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  CreateService();
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::Value(false));

  const GURL kUrl("http://example.com");
  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl, std::nullopt);
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_MsbbDisabled) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  CreateService();
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::Value(false));

  const GURL kUrl("http://example.com");
  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      mock_callback;

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);
  EXPECT_CALL(*mock_observer_,
              OnSuggestionGenerated(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_callback, Run(testing::Eq(std::nullopt)));

  service_->GenerateFilterSuggestions(kTestNavigationId, kUrl,
                                      mock_callback.Get());
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_EmptySupportedTasks) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_client_, GetSupportedTasks(kUrl, _, kTestNavigationId))
      .WillOnce(base::test::RunOnceCallback<1>(std::vector<std::string>()));

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl, std::nullopt);
}

TEST_F(MultistepFilterServiceTest,
       GenerateFilterSuggestions_EmptySupportedTasks) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");
  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      mock_callback;

  EXPECT_CALL(*mock_client_, GetSupportedTasks(kUrl, _, kTestNavigationId))
      .WillOnce(base::test::RunOnceCallback<1>(std::vector<std::string>()));

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);
  EXPECT_CALL(*mock_observer_,
              OnSuggestionGenerated(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_callback, Run(testing::Eq(std::nullopt)));

  service_->GenerateFilterSuggestions(kTestNavigationId, kUrl,
                                      mock_callback.Get());
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

  CreateService(nullptr);
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

}  // namespace multistep_filter
