// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

#include <optional>

#include "base/functional/bind.h"
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

constexpr char kTestId[] = "0";
constexpr char kTestUrl[] = "https://example.com";
constexpr char kTestDomain[] = "example.com";
constexpr char kShoppingTask[] = "SHOPPING";
constexpr char kTestAttributeKey[] = "category";
constexpr char kTestAttributeValue[] = "shoes";
constexpr char kTestSuggestionText[] = "Recall info from previous tabs?";
constexpr char kTestSuggestionUrl[] = "https://example.com/shoes";

using testing::_;
using testing::Return;

class FilterSuggestionGeneratorTest : public testing::Test {
 public:
  FilterSuggestionGeneratorTest() = default;
  ~FilterSuggestionGeneratorTest() override = default;

  void SetUp() override {
    store_ = std::make_unique<FilterStore>();
    generator_ =
        std::make_unique<FilterSuggestionGenerator>(mock_client_, *store_);
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

 private:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<MockAnnotationIndexClient> mock_client_;
  std::unique_ptr<FilterStore> store_;
  std::unique_ptr<FilterSuggestionGenerator> generator_;
};

TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuccessfulSuggestionGenerated) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(), GetSupportedTaskTypesForDomain(kTestDomain, _))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 cb) {
            std::move(cb).Run(std::vector<std::string>{kShoppingTask});
          });

  base::Uuid id = base::Uuid::GenerateRandomV4();
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue}};
  FilterAnnotation annotation(id, kShoppingTask, kTestDomain, base::Time::Now(),
                              attributes);

  base::test::TestFuture<bool> store_future;
  store()->StoreAnnotation(annotation, store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  FilterSuggestionCandidate expected_candidate(
      kTestId, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue)});
  UrlFilterSuggestion expected_suggestion(kTestSuggestionText,
                                          GURL(kTestSuggestionUrl));

  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates(url, _, _))
      .WillOnce(
          [expected_candidate](
              const GURL& u,
              base::span<const FilterAnnotation> filter_annotations,
              base::OnceCallback<void(
                  std::optional<std::vector<FilterSuggestionCandidate>>)> cb) {
            ASSERT_EQ(filter_annotations.size(), 1u);
            EXPECT_EQ(filter_annotations[0].task_type, kShoppingTask);
            std::move(cb).Run(
                std::vector<FilterSuggestionCandidate>{expected_candidate});
          });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback());

  EXPECT_EQ(future.Get(), expected_suggestion);
}

// Tests that `std::nullopt` is returned when the server does not support any
// task types for the given domain.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_NoSupportedTaskTypesReturnsNullopt) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(), GetSupportedTaskTypesForDomain(kTestDomain, _))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 cb) { std::move(cb).Run(std::nullopt); });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `std::nullopt` is returned when the server returns an empty list
// of supported task types for the given domain.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_EmptySupportedTaskTypesReturnsNullopt) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(), GetSupportedTaskTypesForDomain(kTestDomain, _))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 cb) { std::move(cb).Run(std::vector<std::string>()); });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `std::nullopt` is returned when no annotations are found for the
// given domain.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_NoAnnotationsReturnsNullopt) {
  const GURL url(kTestUrl);

  EXPECT_CALL(mock_client(), GetSupportedTaskTypesForDomain(kTestDomain, _))
      .WillOnce(
          [](std::string_view domain,
             base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                 cb) {
            std::move(cb).Run(std::vector<std::string>{kShoppingTask});
          });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
}

}  // namespace

}  // namespace multistep_filter
