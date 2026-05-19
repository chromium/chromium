// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

constexpr char kTestUrl[] = "https://example.com";
constexpr char kTestDomain[] = "example.com";
constexpr char kShoppingTask[] = "SHOPPING";
constexpr char kTestAttributeKey[] = "category";
constexpr char kTestAttributeValue[] = "shoes";
constexpr char16_t kTestAttributeValue16[] = u"shoes";
constexpr char kTestAttributeKey2[] = "size";
constexpr char kTestAttributeValue2[] = "large";
constexpr char16_t kTestAttributeValue2_16[] = u"large";
constexpr char kTestAttributeKey3[] = "color";
constexpr char kTestAttributeValue3[] = "red";
constexpr char16_t kTestAttributeValue3_16[] = u"red";
constexpr char kTestSuggestionUrl[] = "https://example.com/shoes";
constexpr int64_t kTestNavigationId = 12345;

FilterAnnotation CreateDummyAnnotation(
    std::string task_type,
    std::string source_domain,
    std::vector<FilterAttribute> attributes) {
  return FilterAnnotation(base::Uuid::GenerateRandomV4(), std::move(task_type),
                          std::move(source_domain), base::Time::Now(),
                          std::move(attributes));
}

using testing::_;
using testing::Return;

class FilterSuggestionGeneratorTest : public testing::Test {
 public:
  FilterSuggestionGeneratorTest() = default;
  ~FilterSuggestionGeneratorTest() override = default;

  void SetUp() override {
    store_ = std::make_unique<FilterStore>();
    generator_ = std::make_unique<FilterSuggestionGenerator>(
        mock_client_, *store_, /*log_router=*/nullptr);
  }

  void TearDown() override {
    generator_.reset();
    if (store_) {
      store_.reset();
      base::ThreadPoolInstance::Get()->FlushForTesting();
    }
  }

 protected:
  MockAnnotationIndexClient& mock_client() { return mock_client_; }
  FilterStore* store() { return store_.get(); }
  FilterSuggestionGenerator* generator() { return generator_.get(); }
  void DestroyGenerator() { generator_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<MockAnnotationIndexClient> mock_client_;
  std::unique_ptr<FilterStore> store_;
  std::unique_ptr<FilterSuggestionGenerator> generator_;
};

TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuccessfulSuggestionGenerated) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });

  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  FilterSuggestionCandidate expected_candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});
  std::vector<FilterAttributeUiLabel> attribute_ui_labels;
  attribute_ui_labels.emplace_back(expected_candidate.attributes[0],
                                   attributes[0]);
  attribute_ui_labels.emplace_back(expected_candidate.attributes[1],
                                   attributes[1]);
  UrlFilterSuggestion expected_suggestion(
      expected_candidate.navigation_url,
      base::UTF8ToUTF16(annotation.source_domain),
      annotation.creation_timestamp, std::move(attribute_ui_labels),
      kTestNavigationId, /*triggering_domain=*/"example.com");

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([expected_candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        ASSERT_EQ(filter_annotations.size(), 1u);
        EXPECT_EQ(filter_annotations[0].task_type, kShoppingTask);
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{expected_candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), expected_suggestion);
}

TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_FiltersOldAnnotations) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });

  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};

  // Create an old annotation (older than 30 minutes).
  FilterAnnotation old_annotation(
      base::Uuid::GenerateRandomV4(), kShoppingTask, kTestDomain,
      base::Time::Now() - base::Minutes(31), attributes);

  // Create a recent annotation.
  FilterAnnotation recent_annotation(base::Uuid::GenerateRandomV4(),
                                     kShoppingTask, kTestDomain,
                                     base::Time::Now(), attributes);

  base::test::TestFuture<bool> store_future1;
  base::test::TestFuture<bool> store_future2;
  store()->StoreAnnotation(old_annotation, store_future1.GetCallback());
  store()->StoreAnnotation(recent_annotation, store_future2.GetCallback());
  ASSERT_TRUE(store_future1.Get());
  ASSERT_TRUE(store_future2.Get());

  // The candidate matches the recent annotation.
  FilterSuggestionCandidate candidate(
      recent_annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate, recent_annotation](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        // Verify that the old annotation is NOT passed to the client.
        EXPECT_EQ(filter_annotations.size(), 1u);
        EXPECT_EQ(filter_annotations[0].id, recent_annotation.id);
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  ASSERT_TRUE(future.Get().has_value());
}

TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuppressesSubsumedSuggestions) {
  const GURL url("https://example.com/search?category=shoes&size=large");

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });

  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  // Identical URL should be suppressed.
  FilterSuggestionCandidate candidate(
      annotation.id,
      GURL("https://example.com/search?category=shoes&size=large"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuppressesSubsetParameters) {
  const GURL url("https://example.com/search?category=shoes&size=large");
  EXPECT_CALL(mock_client(), GetSupportedTaskTypesForDomain)
      .WillOnce(
          [](std::string_view,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);
  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  // Candidate URL is a subset of parameters of the triggering URL.
  FilterSuggestionCandidate candidate(
      annotation.id, GURL("https://example.com/search?category=shoes"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});
  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates)
      .WillOnce([candidate](
                    const GURL&, base::span<const FilterAnnotation>,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);
  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_DoesNotSuppressDifferentBaseUrl) {
  const GURL url("https://example.com/search?category=shoes&size=large");
  EXPECT_CALL(mock_client(), GetSupportedTaskTypesForDomain)
      .WillOnce(
          [](std::string_view,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2},
      {kTestAttributeKey3, kTestAttributeValue3}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);
  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  // Different base URL should NOT be suppressed!
  FilterSuggestionCandidate candidate(
      annotation.id,
      GURL("https://example.com/other?category=shoes&size=large"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey3,
                                          kTestAttributeValue3_16)});
  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates)
      .WillOnce([candidate](
                    const GURL&, base::span<const FilterAnnotation>,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);
  EXPECT_TRUE(future.Get().has_value());
}

TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_DoesNotSuppressAdditionalParameters) {
  const GURL url("https://example.com/search?category=shoes&size=large");
  EXPECT_CALL(mock_client(), GetSupportedTaskTypesForDomain)
      .WillOnce(
          [](std::string_view,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2},
      {kTestAttributeKey3, kTestAttributeValue3}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);
  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  // Additional parameters should NOT be suppressed!
  FilterSuggestionCandidate candidate(
      annotation.id,
      GURL("https://example.com/search?category=shoes&size=large&sort=new"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey3,
                                          kTestAttributeValue3_16)});
  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates)
      .WillOnce([candidate](
                    const GURL&, base::span<const FilterAnnotation>,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);
  EXPECT_TRUE(future.Get().has_value());
}

TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuppressesOneAttribute) {
  const GURL url("https://example.com/search?category=shoes&size=large");
  EXPECT_CALL(mock_client(), GetSupportedTaskTypesForDomain)
      .WillOnce(
          [](std::string_view,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);
  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  // Candidate has exactly 1 attribute! So it should be suppressed!
  FilterSuggestionCandidate candidate(
      annotation.id,
      GURL("https://example.com/search?category=shoes&size=large&sort=new"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16)});
  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates)
      .WillOnce([candidate](
                    const GURL&, base::span<const FilterAnnotation>,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that only attributes with matching keys in the annotation are included
// in the suggestion, following the order in the candidate.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_OnlyMatchesPresentKeys) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });

  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain,
                            {{kTestAttributeKey, kTestAttributeValue},
                             {kTestAttributeKey3, kTestAttributeValue3}});

  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  // Candidate has key2 (missing in annotation) and key1 (present).
  FilterSuggestionCandidate candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey3,
                                          kTestAttributeValue3_16)});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  std::optional<UrlFilterSuggestion> result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->attribute_ui_labels.size(), 2u);
  EXPECT_EQ(result->attribute_ui_labels[0].attribute_label,
            kTestAttributeValue16);
  EXPECT_EQ(result->attribute_ui_labels[0].attribute_value,
            kTestAttributeValue16);
  EXPECT_EQ(result->attribute_ui_labels[1].attribute_label,
            kTestAttributeValue3_16);
  EXPECT_EQ(result->attribute_ui_labels[1].attribute_value,
            kTestAttributeValue3_16);
}

// Tests that the suggestion is generated with empty attributes if no keys
// match between the candidate and the annotation.
TEST_F(FilterSuggestionGeneratorTest, GenerateSuggestion_NoMatchingKeys) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });

  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, {{"key1", "val1"}});

  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  FilterSuggestionCandidate candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute("key2", u"label2")});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `std::nullopt` is returned when the server does not support any
// task types for the given domain.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_NoSupportedTaskTypesReturnsNullopt) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) { std::move(callback).Run(std::nullopt); });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `std::nullopt` is returned when the server returns an empty list
// of supported task types for the given domain.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_EmptySupportedTaskTypesReturnsNullopt) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) {
            std::move(callback).Run(std::vector<std::string>());
          });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `std::nullopt` is returned when no annotations are found for the
// given domain.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_NoAnnotationsReturnsNullopt) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `std::nullopt` is returned when a candidate is returned but no
// matching annotation is found in the list of annotations sent to the server.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_CandidateWithNoMatchingAnnotationReturnsNullopt) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 callback,
             int64_t navigation_id) {
            std::move(callback).Run(std::vector<std::string>{kShoppingTask});
          });

  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  // Create a candidate with a non-matching annotation ID.
  FilterSuggestionCandidate candidate(
      base::Uuid::GenerateRandomV4(), GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16)});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that the callback is invoked with `std::nullopt` if the underlying
// client drops the callback without running it.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_CallbackInvokedWhenClientDropsIt) {
  const GURL url(kTestUrl);
  base::OnceCallback<void(std::optional<std::vector<std::string>>)> captured_cb;
  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))
      .WillOnce(
          [&](std::string_view domain,
              base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                  callback,
              int64_t navigation_id) { captured_cb = std::move(callback); });
  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;

  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  ASSERT_FALSE(future.IsReady());

  captured_cb.Reset();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that the callback is invoked with `std::nullopt` if the
// `FilterSuggestionGenerator` is destroyed while a request is pending.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_CallbackInvokedWhenGeneratorDestroyed) {
  const GURL url(kTestUrl);
  base::OnceCallback<void(std::optional<std::vector<std::string>>)> captured_cb;
  EXPECT_CALL(mock_client(),
              GetSupportedTaskTypesForDomain(kTestDomain, _, kTestNavigationId))

      .WillOnce(
          [&](std::string_view domain,
              base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                  callback,
              int64_t navigation_id) {
            // Capture the callback but do NOT run it.
            captured_cb = std::move(callback);
          });
  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;

  generator()->GenerateSuggestion(url, future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  ASSERT_FALSE(future.IsReady());

  // The callback is bound to a `WeakPtr` of the generator, so the `WeakPtr` is
  // now invalidated.
  DestroyGenerator();
  captured_cb.Reset();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

}  // namespace

}  // namespace multistep_filter
