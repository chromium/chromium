// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_query_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::accessibility_annotator::MemoryDataProvider;
using ::accessibility_annotator::MemorySearchResult;
using ::accessibility_annotator::MemorySearchResults;
using ::accessibility_annotator::MemorySearchStatus;
using ::accessibility_annotator::QueryIntentType;

class FakeMemoryDataProvider : public MemoryDataProvider {
 public:
  std::vector<MemorySearchResult> RetrieveAll(QueryIntentType type) override {
    last_type_ = type;
    return results_;
  }
  void set_results(std::vector<MemorySearchResult> results) {
    results_ = std::move(results);
  }
  QueryIntentType last_type() const { return last_type_; }

 private:
  std::vector<MemorySearchResult> results_;
  QueryIntentType last_type_ = QueryIntentType::kUnknown;
};

class AccessibilityQueryServiceTest : public testing::Test {
 public:
  AccessibilityQueryServiceTest() = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests that the query service returns the expected results when the intent is
// successfully classified.
TEST_F(AccessibilityQueryServiceTest, Query_Success) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service =
      std::make_unique<AccessibilityQueryService>(std::move(data_provider));

  MemorySearchResult result(QueryIntentType::kNameFull, u"Name", u"John Doe");
  fake_data_provider->set_results({result});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get().entries.size(), 1u);
  EXPECT_EQ(future.Get().entries[0].value, u"John Doe");
  EXPECT_EQ(fake_data_provider->last_type(), QueryIntentType::kNameFull);
}

// Tests that the query service returns an empty list when the intent is
// unknown.
TEST_F(AccessibilityQueryServiceTest, Query_UnknownIntent) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto service =
      std::make_unique<AccessibilityQueryService>(std::move(data_provider));

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().entries.empty());
  EXPECT_EQ(future.Get().status, MemorySearchStatus::kUnsupportedQuery);
}

// Tests that the query service correctly filters results when filter words
// are present in the query.
TEST_F(AccessibilityQueryServiceTest, Query_WithFilterWords) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service =
      std::make_unique<AccessibilityQueryService>(std::move(data_provider));

  MemorySearchResult entry1(QueryIntentType::kAddressFull, u"Address",
                            u"123 San Diego St Home San Diego");
  MemorySearchResult entry2(QueryIntentType::kAddressFull, u"Address",
                            u"456 Mountain View Rd Work Mountain View");

  fake_data_provider->set_results({entry1, entry2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in San Diego",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get().entries.size(), 1u);
  EXPECT_EQ(future.Get().entries[0].value, u"123 San Diego St Home San Diego");
  EXPECT_EQ(fake_data_provider->last_type(), QueryIntentType::kAddressFull);
}

// Tests that the query service falls back to returning all results for the
// classified intent if none of the results match the filter words.
TEST_F(AccessibilityQueryServiceTest,
       Query_WithFilterWords_NoMatch_ReturnsAll) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service =
      std::make_unique<AccessibilityQueryService>(std::move(data_provider));

  MemorySearchResult entry(QueryIntentType::kAddressFull, u"Address",
                           u"123 San Diego St Home San Diego");
  fake_data_provider->set_results({entry});

  // "New York" won't match "San Diego", so it should fallback to returning all
  // results for that intent.
  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in New York",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get().entries.size(), 1u);
  EXPECT_EQ(future.Get().entries[0].value, u"123 San Diego St Home San Diego");
  EXPECT_EQ(fake_data_provider->last_type(), QueryIntentType::kAddressFull);
}

}  // namespace

}  // namespace accessibility_annotator
