// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_query_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/accessibility_query_service_delegate.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::accessibility_annotator::EntryType;

class MockAccessibilityQueryServiceDelegate
    : public AccessibilityQueryServiceDelegate {
 public:
  MOCK_METHOD(void,
              RetrieveLiveTabContext,
              (LiveTabContextQuery query,
               base::OnceCallback<void(LiveTabContextResponse)> callback),
              (override));
};

class FakeMemoryDataProvider : public MemoryDataProvider {
 public:
  void RetrieveAll(EntryType type,
                   base::OnceCallback<void(std::vector<MemorySearchResult>)>
                       callback) override {
    last_type_ = type;
    std::move(callback).Run(results_);
  }
  void SetResults(std::vector<MemorySearchResult> results) {
    results_ = std::move(results);
  }
  EntryType last_type() const { return last_type_; }

  std::string_view GetHistogramSuffix() const override {
    return "FakeMemoryDataProvider";
  }

 private:
  std::vector<MemorySearchResult> results_;
  EntryType last_type_ = EntryType::kUnknown;
};

class FakeOnePResolver : public OnePResolver {
 public:
  void Query(std::u16string query, QueryCallback callback) override {
    last_query_ = query;
    std::move(callback).Run(results_);
  }

  void set_results(std::vector<MemorySearchResult> results) {
    results_ = std::move(results);
  }

  std::u16string last_query() const { return last_query_; }

 private:
  std::u16string last_query_;
  std::vector<MemorySearchResult> results_;
};

class DelayedMemoryDataProvider : public MemoryDataProvider {
 public:
  void RetrieveAll(EntryType type,
                   base::OnceCallback<void(std::vector<MemorySearchResult>)>
                       callback) override {
    callbacks_.push_back(std::move(callback));
  }
  void CompleteNext() {
    if (!callbacks_.empty()) {
      std::move(callbacks_.front()).Run({});
      callbacks_.erase(callbacks_.begin());
    }
  }

  std::string_view GetHistogramSuffix() const override {
    return "DelayedMemoryDataProvider";
  }

 private:
  std::vector<base::OnceCallback<void(std::vector<MemorySearchResult>)>>
      callbacks_;
};

class AccessibilityQueryServiceTest : public testing::Test {
 public:
  AccessibilityQueryServiceTest() = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests that the query service returns an internal failure status after
// shutdown.
TEST_F(AccessibilityQueryServiceTest, Query_AfterShutdown) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::make_unique<FakeMemoryDataProvider>());
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  service->Shutdown();

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kInternalFailure);
}

// Tests that the query service returns an internal failure status when no
// providers are available.
TEST_F(AccessibilityQueryServiceTest, Query_NoProviders) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kInternalFailure);
}

// Tests that the query service queries multiple providers and combines results.
TEST_F(AccessibilityQueryServiceTest, Query_MultipleProviders) {
  auto data_provider1 = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider1 = data_provider1.get();
  auto data_provider2 = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider2 = data_provider2.get();

  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider1));
  providers.push_back(std::move(data_provider2));
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult result1(EntryType::kNameFull, u"Name", u"John Doe");
  fake_data_provider1->SetResults({result1});
  MemorySearchResult result2(EntryType::kNameFull, u"Name", u"Jane Doe");
  fake_data_provider2->SetResults({result2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries,
              testing::ElementsAre(
                  testing::Field(&MemorySearchResult::value, u"John Doe"),
                  testing::Field(&MemorySearchResult::value, u"Jane Doe")));
  EXPECT_EQ(fake_data_provider1->last_type(), EntryType::kNameFull);
  EXPECT_EQ(fake_data_provider2->last_type(), EntryType::kNameFull);
}

// Tests that the query service returns the expected results when the intent is
// successfully classified.
TEST_F(AccessibilityQueryServiceTest, Query_Success) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider));
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult result(EntryType::kNameFull, u"Name", u"John Doe");
  fake_data_provider->SetResults({result});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& search_results = future.Get();
  EXPECT_THAT(search_results.entries,
              testing::ElementsAre(
                  testing::Field(&MemorySearchResult::value, u"John Doe")));
  EXPECT_EQ(fake_data_provider->last_type(), EntryType::kNameFull);
}

// Tests that the query service returns an empty list when the intent is
// unknown and there is no 1p resolver available.
TEST_F(AccessibilityQueryServiceTest, Query_UnknownIntent_NoOnePResolver) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::make_unique<FakeMemoryDataProvider>());
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kUnsupportedQuery);
}

// Tests that the query service returns unsupported query when the intent is
// unknown and the 1P resolver returns nothing.
TEST_F(AccessibilityQueryServiceTest,
       Query_UnknownIntent_OnePResolverEmpty_ReturnsUnsupported) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::make_unique<FakeMemoryDataProvider>());

  auto one_p_resolver = std::make_unique<FakeOnePResolver>();
  auto* fake_one_p_resolver = one_p_resolver.get();

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), std::move(one_p_resolver),
      /*remote_model_executor=*/nullptr);

  fake_one_p_resolver->set_results({});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", /*full_search=*/true,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kUnsupportedQuery);
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(fake_one_p_resolver->last_query(), u"random query");
}

// Tests that the query service queries the 1P resolver when the intent is
// unknown.
TEST_F(AccessibilityQueryServiceTest, Query_UnknownIntent_QueriesOnePResolver) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::make_unique<FakeMemoryDataProvider>());

  auto one_p_resolver = std::make_unique<FakeOnePResolver>();
  auto* fake_one_p_resolver = one_p_resolver.get();

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), std::move(one_p_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult one_p_entry(EntryType::kUnknown, u"Custom Type",
                                 u"Some 1P Value");
  fake_one_p_resolver->set_results({one_p_entry});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", /*full_search=*/true,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries,
              testing::ElementsAre(testing::Field(&MemorySearchResult::value,
                                                  u"Some 1P Value")));
  EXPECT_EQ(fake_one_p_resolver->last_query(), u"random query");
}

// Tests that the query service returns empty success when no local data is
// found for a known intent and there is no 1P resolver.
TEST_F(AccessibilityQueryServiceTest, Query_NoLocalData_NoOnePResolver) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::make_unique<FakeMemoryDataProvider>());

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_TRUE(result.entries.empty());
}

// Tests that the query service queries the 1P resolver when no local data
// is found for a known intent.
TEST_F(AccessibilityQueryServiceTest, Query_NoLocalData_QueriesOnePResolver) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::make_unique<FakeMemoryDataProvider>());

  auto one_p_resolver = std::make_unique<FakeOnePResolver>();
  auto* fake_one_p_resolver = one_p_resolver.get();

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), std::move(one_p_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult one_p_entry(EntryType::kNameFull, u"Name", u"Jane Doe");
  fake_one_p_resolver->set_results({one_p_entry});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/true,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value, u"Jane Doe")));
  EXPECT_EQ(fake_one_p_resolver->last_query(), u"what is my name");
}

// Tests that the query service does NOT query the 1P resolver when no local
// data is found if full search is disabled.
TEST_F(AccessibilityQueryServiceTest,
       Query_NoLocalData_FullSearchFalse_NoOnePQuery) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::make_unique<FakeMemoryDataProvider>());

  auto one_p_resolver = std::make_unique<FakeOnePResolver>();
  auto* fake_one_p_resolver = one_p_resolver.get();

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), std::move(one_p_resolver),
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_TRUE(result.entries.empty());
  EXPECT_TRUE(fake_one_p_resolver->last_query().empty());
}

// Tests that the query service returns success with an empty list when no local
// data is found and the 1P resolver also returns nothing.
TEST_F(AccessibilityQueryServiceTest,
       Query_NoLocalData_OnePResolverEmpty_ReturnsEmpty) {
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::make_unique<FakeMemoryDataProvider>());

  auto one_p_resolver = std::make_unique<FakeOnePResolver>();
  auto* fake_one_p_resolver = one_p_resolver.get();

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), std::move(one_p_resolver),
      /*remote_model_executor=*/nullptr);

  fake_one_p_resolver->set_results({});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/true,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(fake_one_p_resolver->last_query(), u"what is my name");
}

// Tests that the query service correctly filters results when filter words
// are present in the query.
TEST_F(AccessibilityQueryServiceTest, Query_WithFilterWords) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider));
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult entry1(EntryType::kAddressFull, u"Address",
                            u"123 San Diego St Home San Diego");
  MemorySearchResult entry2(EntryType::kAddressFull, u"Address",
                            u"456 Mountain View Rd Work Mountain View");

  fake_data_provider->SetResults({entry1, entry2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in San Diego", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"123 San Diego St Home San Diego")));
  EXPECT_EQ(fake_data_provider->last_type(), EntryType::kAddressFull);
}

// Tests that the query service falls back to returning all results for the
// classified intent if none of the results match the filter words.
TEST_F(AccessibilityQueryServiceTest,
       Query_WithFilterWords_NoMatch_ReturnsAll) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider));
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult entry(EntryType::kAddressFull, u"Address",
                           u"123 San Diego St Home San Diego");
  fake_data_provider->SetResults({entry});

  // "New York" won't match "San Diego", so it should fallback to returning all
  // results for that intent.
  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in New York", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"123 San Diego St Home San Diego")));
  EXPECT_EQ(fake_data_provider->last_type(), EntryType::kAddressFull);
}

// Tests that the query service records the provider result count metric.
TEST_F(AccessibilityQueryServiceTest, RecordsProviderResultCountMetric) {
  base::HistogramTester histogram_tester;

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider));
  auto service = std::make_unique<AccessibilityQueryService>(
      /*delegate=*/nullptr, std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult result1(EntryType::kNameFull, u"Name", u"John Doe");
  MemorySearchResult result2(EntryType::kNameFull, u"Name", u"Jane Doe");
  fake_data_provider->SetResults({result1, result2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.AccessibilityQueryService.ProviderResultCount."
      "FakeMemoryDataProvider",
      /*sample=*/2, /*expected_bucket_count=*/1);
}

// Tests that the query service queries the 1P resolver if local filtering
// removes all results.
TEST_F(AccessibilityQueryServiceTest,
       Query_WithFilterWords_NoMatch_QueriesOnePResolver) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider));

  auto one_p_resolver = std::make_unique<FakeOnePResolver>();
  auto* fake_one_p_resolver = one_p_resolver.get();

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), std::move(one_p_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult local_entry(EntryType::kAddressFull, u"Address",
                                 u"123 San Diego St Home San Diego");
  fake_data_provider->SetResults({local_entry});

  MemorySearchResult one_p_entry(EntryType::kAddressFull, u"Address",
                                 u"456 New York Ave Home New York");
  fake_one_p_resolver->set_results({one_p_entry});

  // "New York" won't match the local "San Diego" address, so it should
  // fallback to querying the 1P resolver.
  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in New York", /*full_search=*/true,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"456 New York Ave Home New York")));
  EXPECT_EQ(fake_one_p_resolver->last_query(),
            u"What's my home address in New York");
}

// Tests that the query service falls back to original local entries if the 1P
// resolver returns no results.
TEST_F(AccessibilityQueryServiceTest,
       Query_WithFilterWords_NoMatch_OnePResolverEmpty_ReturnsLocal) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider));

  auto one_p_resolver = std::make_unique<FakeOnePResolver>();
  auto* fake_one_p_resolver = one_p_resolver.get();

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), std::move(one_p_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult local_entry(EntryType::kAddressFull, u"Address",
                                 u"123 San Diego St Home San Diego");
  fake_data_provider->SetResults({local_entry});

  // The 1P resolver returns nothing.
  fake_one_p_resolver->set_results({});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in New York", /*full_search=*/true,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"123 San Diego St Home San Diego")));
  EXPECT_EQ(fake_one_p_resolver->last_query(),
            u"What's my home address in New York");
}

// Tests that the query service does NOT query the 1P resolver when full search
// is enabled if local data is found.
TEST_F(AccessibilityQueryServiceTest, Query_FullSearch_NoOnePIfLocalDataFound) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider));

  auto one_p_resolver = std::make_unique<FakeOnePResolver>();
  auto* fake_one_p_resolver = one_p_resolver.get();

  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), std::move(one_p_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult local_entry(EntryType::kNameFull, u"Name", u"John Doe");
  fake_data_provider->SetResults({local_entry});

  MemorySearchResult one_p_entry(EntryType::kNameFull, u"Name", u"Jane Doe");
  fake_one_p_resolver->set_results({one_p_entry});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/true,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  // Should return the local result and NOT query 1P.
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value, u"John Doe")));
  EXPECT_TRUE(fake_one_p_resolver->last_query().empty());
}

// Tests that the query service does not send results for a query that has been
// superseded by a newer query.
TEST_F(AccessibilityQueryServiceTest, StaleResultsAreNotSent) {
  auto data_provider = std::make_unique<DelayedMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider));
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future1;
  service->Query(u"what is my name", /*full_search=*/false,
                 future1.GetRepeatingCallback());

  // Start a second query before the first one completes.
  base::test::TestFuture<MemorySearchResults> future2;
  service->Query(u"what is my address", /*full_search=*/false,
                 future2.GetRepeatingCallback());

  // Complete the first query's data retrieval.
  fake_data_provider->CompleteNext();

  // The first query's callback should NOT be called.
  EXPECT_FALSE(future1.IsReady());

  // Complete the second query's data retrieval.
  fake_data_provider->CompleteNext();

  // The second query's callback should be called.
  ASSERT_TRUE(future2.Wait());
}

// Tests that deduplication preserves the original insertion order.
TEST_F(AccessibilityQueryServiceTest,
       Query_DeduplicatesResults_PreservesOrder) {
  auto data_provider1 = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider1 = data_provider1.get();

  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider1));
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult result1(EntryType::kNameFull, u"Name", u"Alice");
  MemorySearchResult result2(EntryType::kNameFull, u"Name", u"Bob");
  MemorySearchResult result3(EntryType::kNameFull, u"Name",
                             u"Alice");  // duplicate of result1
  MemorySearchResult result4(EntryType::kNameFull, u"Name", u"Charlie");

  fake_data_provider1->SetResults({result1, result2, result3, result4});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const MemorySearchResults& result = future.Get();
  EXPECT_THAT(result.entries,
              testing::ElementsAre(
                  testing::Field(&MemorySearchResult::value, u"Alice"),
                  testing::Field(&MemorySearchResult::value, u"Bob"),
                  testing::Field(&MemorySearchResult::value, u"Charlie")));
}

// Tests that deduplication retains fields like confidence_score from the first
// entry and merges sources.
TEST_F(AccessibilityQueryServiceTest,
       Query_DeduplicatesResults_RetainsFirstEntryFieldsAndMergesSources) {
  auto data_provider1 = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider1 = data_provider1.get();

  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider1));
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  EntryMetadata metadata(EntryType::kAddressCity, u"City", u"San Diego");

  MemorySearchResult result1(EntryType::kNameFull, u"Name", u"John Doe",
                             /*confidence_score=*/0.9);
  result1.metadata_list.push_back(metadata);
  result1.sources.push_back(
      MemoryEntrySource(MemoryEntrySourceType::kAutofill));

  MemorySearchResult result2(EntryType::kNameFull, u"Name", u"John Doe",
                             /*confidence_score=*/0.5);
  result2.metadata_list.push_back(metadata);
  result2.sources.push_back(MemoryEntrySource(MemoryEntrySourceType::kGmail));
  // Duplicate source shouldn't be added twice.
  result2.sources.push_back(
      MemoryEntrySource(MemoryEntrySourceType::kAutofill));

  fake_data_provider1->SetResults({result1, result2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].value, u"John Doe");
  EXPECT_DOUBLE_EQ(result.entries[0].confidence_score, 0.9);
  ASSERT_EQ(result.entries[0].sources.size(), 2u);
  EXPECT_EQ(result.entries[0].sources[0].type,
            MemoryEntrySourceType::kAutofill);
  EXPECT_EQ(result.entries[0].sources[1].type, MemoryEntrySourceType::kGmail);
}

// Tests that entries with different values or metadata lists are both retained.
TEST_F(AccessibilityQueryServiceTest,
       Query_DeduplicatesResults_KeepsDifferentEntries) {
  auto data_provider1 = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider1 = data_provider1.get();

  std::vector<std::unique_ptr<MemoryDataProvider>> providers;
  providers.push_back(std::move(data_provider1));
  auto service = std::make_unique<AccessibilityQueryService>(
      std::make_unique<MockAccessibilityQueryServiceDelegate>(),
      std::move(providers), /*one_p_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  EntryMetadata metadata_sd(EntryType::kAddressCity, u"City", u"San Diego");
  EntryMetadata metadata_ny(EntryType::kAddressCity, u"City", u"New York");

  // Same value, different metadata
  MemorySearchResult result1(EntryType::kNameFull, u"Name", u"John Doe");
  result1.metadata_list.push_back(metadata_sd);

  MemorySearchResult result2(EntryType::kNameFull, u"Name", u"John Doe");
  result2.metadata_list.push_back(metadata_ny);

  // Different value, same metadata
  MemorySearchResult result3(EntryType::kNameFull, u"Name", u"Jane Doe");
  result3.metadata_list.push_back(metadata_sd);

  // Same value and metadata, different type
  MemorySearchResult result4(EntryType::kUnknown, u"Unknown", u"John Doe");
  result4.metadata_list.push_back(metadata_sd);

  fake_data_provider1->SetResults({result1, result2, result3, result4});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", /*full_search=*/false,
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ASSERT_EQ(result.entries.size(), 4u);
  EXPECT_EQ(result.entries[0].value, u"John Doe");
  ASSERT_EQ(result.entries[0].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[0].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[0].type, EntryType::kNameFull);

  EXPECT_EQ(result.entries[1].value, u"John Doe");
  ASSERT_EQ(result.entries[1].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[1].metadata_list[0].value, u"New York");
  EXPECT_EQ(result.entries[1].type, EntryType::kNameFull);

  EXPECT_EQ(result.entries[2].value, u"Jane Doe");
  ASSERT_EQ(result.entries[2].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[2].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[2].type, EntryType::kNameFull);

  EXPECT_EQ(result.entries[3].value, u"John Doe");
  ASSERT_EQ(result.entries[3].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[3].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[3].type, EntryType::kUnknown);
}

}  // namespace

}  // namespace accessibility_annotator
