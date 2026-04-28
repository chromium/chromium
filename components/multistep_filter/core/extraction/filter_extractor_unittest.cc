// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/extraction/filter_extractor.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

namespace {

using ::testing::_;

constexpr char kTestDomain[] = "example.com";
constexpr char kTestAttributeKey[] = "category";
constexpr char kTestAttributeValue[] = "shoes";
constexpr char kTestUrl[] = "https://example.com/search?q=shoes";
constexpr char kTestTask[] = "SHOPPING";
constexpr int64_t kTestNavigationId = 12345;

class MockFilterStore : public FilterStore {
 public:
  MockFilterStore() = default;
  ~MockFilterStore() override = default;

  MOCK_METHOD(void,
              StoreAnnotation,
              (const FilterAnnotation& annotation,
               base::OnceCallback<void(bool)> callback),
              (override));
};

class FilterExtractorTest : public ::testing::Test {
 public:
  FilterExtractorTest() = default;
  ~FilterExtractorTest() override = default;

  MockAnnotationIndexClient& mock_client() { return mock_client_; }
  MockFilterStore& filter_store() { return filter_store_; }
  FilterExtractor& extractor() { return extractor_; }

 private:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<MockAnnotationIndexClient> mock_client_;
  testing::NiceMock<MockFilterStore> filter_store_;
  FilterExtractor extractor_{mock_client_, filter_store_,
                             /*log_router=*/nullptr};
};

// Test that the extractor successfully extracts and stores an annotation.
TEST_F(FilterExtractorTest, ExtractAnnotationFromUrl_Success) {
  GURL test_url(kTestUrl);
  base::Uuid id = base::Uuid::GenerateRandomV4();
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue}};
  FilterAnnotation annotation(id, kTestTask, kTestDomain, base::Time::Now(),
                              attributes);

  EXPECT_CALL(mock_client(), ExtractFilterAnnotation(test_url, _))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));
  EXPECT_CALL(filter_store(), StoreAnnotation(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  base::test::TestFuture<std::optional<base::Uuid>> extract_future;
  extractor().ExtractAnnotationFromUrl(test_url, extract_future.GetCallback(),
                                       kTestNavigationId, kTestDomain);

  std::optional<base::Uuid> annotation_id = extract_future.Get();
  ASSERT_TRUE(annotation_id.has_value());
  EXPECT_EQ(annotation_id.value(), id);
}

// Test that the extractor return nullopt if FilterStore::StoreAnnotation fails.
TEST_F(FilterExtractorTest, ExtractAnnotationFromUrl_StoreFailed) {
  GURL test_url(kTestUrl);
  base::Uuid id = base::Uuid::GenerateRandomV4();
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue}};
  FilterAnnotation annotation(id, kTestTask, kTestDomain, base::Time::Now(),
                              attributes);

  EXPECT_CALL(mock_client(), ExtractFilterAnnotation(test_url, _))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));
  EXPECT_CALL(filter_store(), StoreAnnotation(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(false));

  base::test::TestFuture<std::optional<base::Uuid>> extract_future;
  extractor().ExtractAnnotationFromUrl(test_url, extract_future.GetCallback(),
                                       kTestNavigationId, kTestDomain);

  std::optional<base::Uuid> annotation_id = extract_future.Get();
  EXPECT_FALSE(annotation_id.has_value());
}

// Test that the extractor does not store anything if the annotation index
// client returns nullopt.
TEST_F(FilterExtractorTest, ExtractAnnotationFromUrl_EmptyResult) {
  GURL test_url(kTestUrl);

  EXPECT_CALL(mock_client(), ExtractFilterAnnotation(test_url, _))
      .WillOnce(base::test::RunOnceCallback<1>(std::nullopt));

  base::test::TestFuture<std::optional<base::Uuid>> extract_future;
  extractor().ExtractAnnotationFromUrl(test_url, extract_future.GetCallback(),
                                       kTestNavigationId, kTestDomain);

  std::optional<base::Uuid> annotation_id = extract_future.Get();
  EXPECT_FALSE(annotation_id.has_value());
}

}  // namespace

}  // namespace multistep_filter
