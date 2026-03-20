// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_service.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service_test_api.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

using ::testing::_;

class MockFilterExtractor : public FilterExtractor {
 public:
  MockFilterExtractor(AnnotationIndexClient& annotation_index_client,
                      FilterStore& filter_store)
      : FilterExtractor(annotation_index_client, filter_store) {}
  MOCK_METHOD(void, ExtractAnnotationFromUrl, (const GURL& url), (override));
};

class MockFilterSuggestionGenerator : public FilterSuggestionGenerator {
 public:
  MockFilterSuggestionGenerator(AnnotationIndexClient& annotation_index_client,
                                FilterStore& filter_store)
      : FilterSuggestionGenerator(annotation_index_client, filter_store) {}
  MOCK_METHOD(
      void,
      GenerateSuggestion,
      (const GURL& url,
       base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback),
      (override));
};

class MultistepFilterServiceTest : public testing::Test {
 public:
  MultistepFilterServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(kMultistepFilter);
  }

  void CreateService() {
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

    service_ = std::make_unique<MultistepFilterService>(
        std::move(annotation_index_client), std::move(filter_store),
        identity_test_env_.identity_manager());

    test_api(*service_).set_filter_extractor(std::move(filter_extractor));
    test_api(*service_).set_filter_suggestion_generator(
        std::move(filter_suggestion_generator));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

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

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl(kUrl));

  service_->ExtractAnnotation(kUrl);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_NotSignedIn) {
  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);

  service_->ExtractAnnotation(kUrl);
}

TEST_F(MultistepFilterServiceTest, ExtractAnnotation_NotAllowedDomain) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "example.com"}});

  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://notexample.com");

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl).Times(0);
  service_->ExtractAnnotation(kUrl);
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_generator_, GenerateSuggestion(kUrl, _));

  service_->GenerateFilterSuggestions(kUrl, base::DoNothing());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_NotSignedIn) {
  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);

  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt)));

  service_->GenerateFilterSuggestions(kUrl, callback.Get());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_NotAllowedDomain) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter, {{"allowed_domains", "example.com"}});

  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://notexample.com");

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);

  base::MockCallback<
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>>
      callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt)));

  service_->GenerateFilterSuggestions(kUrl, callback.Get());
}

TEST_F(MultistepFilterServiceTest, GenerateFilterSuggestions_NullCallback) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  CreateService();
  const GURL kUrl("http://example.com");

  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);

  service_->GenerateFilterSuggestions(kUrl, base::NullCallback());
}

}  // namespace multistep_filter
