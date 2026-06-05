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
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service_test_api.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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
  MOCK_METHOD(void,
              ExtractAnnotationFromUrl,
              (const GURL& url,
               base::OnceCallback<void(std::optional<base::Uuid>)> callback,
               int64_t navigation_id,
               std::string_view domain),
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
       base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
       int64_t navigation_id,
       std::string_view domain),
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
  }

  void CreateService(signin::IdentityManager* identity_manager) {
    auto annotation_index_client =
        std::make_unique<MockAnnotationIndexClient>();
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

  std::unique_ptr<MockObserver> mock_observer_;
  std::unique_ptr<MultistepFilterService> service_;

  // Raw pointers to the mocks, valid as long as the service is alive.
  raw_ptr<MockFilterExtractor> mock_extractor_ = nullptr;
  raw_ptr<MockFilterSuggestionGenerator> mock_generator_ = nullptr;
};

TEST_F(MultistepFilterServiceTest, CreateAndDestroy) {
  // Verifies the service can be created and destroyed without crashing.
  CreateService();
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");
  base::Uuid mock_uuid = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl(
                                    kUrl, _, kTestNavigationId, "example.com"))
      .WillOnce(base::test::RunOnceCallback<1>(mock_uuid));

  EXPECT_CALL(*mock_observer_,
              OnExtractionFinished(testing::Optional(mock_uuid)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_NotSignedIn) {
  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_NullIdentityManager) {
  CreateService(nullptr);
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_NotAllowedDomain) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "example.com"}});

  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://notexample.com");

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(*mock_observer_, OnExtractionFinished(testing::Eq(std::nullopt)));

  service_->ExtractAnnotation(kTestNavigationId, kUrl);
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");
  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      mock_callback;
  UrlFilterSuggestion mock_suggestion(
      UrlFilterSuggestion::Params{.navigation_url = kUrl,
                                  .source_domain = u"example.com",
                                  .extraction_timestamp = base::Time::Now(),
                                  .attribute_ui_labels = {},
                                  .triggering_navigation_id = kTestNavigationId,
                                  .triggering_domain = "example.com",
                                  .task_type = "task1"});

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(kUrl, _, kTestNavigationId, "example.com"))
      .WillOnce(base::test::RunOnceCallback<1>(mock_suggestion));

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

  service_->ExtractAnnotation(kTestNavigationId, kUrl);
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

}  // namespace multistep_filter
