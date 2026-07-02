// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/at_memory_query_service.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/at_memory_query_service_delegate.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::accessibility_annotator::MemoryDataType;
using ::testing::_;
using ::testing::Field;
using ::testing::UnorderedElementsAre;

class MockAtMemoryQueryServiceDelegate : public AtMemoryQueryServiceDelegate {
 public:
  MOCK_METHOD(void,
              RetrieveLiveTabContext,
              (LiveTabContextQuery query,
               base::OnceCallback<void(LiveTabContextResponse)> callback),
              (override));
};

class FakeMemoryDataProvider : public MemoryDataProvider {
 public:
  void RetrieveAll(const std::vector<MemoryDataType>& types,
                   base::OnceCallback<void(std::vector<MemorySearchResult>)>
                       callback) override {
    last_types_ = types;
    std::move(callback).Run(results_);
  }
  void SetResults(std::vector<MemorySearchResult> results) {
    results_ = std::move(results);
  }
  const std::vector<MemoryDataType>& last_types() const { return last_types_; }
  MemoryDataType last_type() const {
    return last_types_.empty() ? MemoryDataType::kUnknown : last_types_[0];
  }

 private:
  std::vector<MemorySearchResult> results_;
  std::vector<MemoryDataType> last_types_;
};

class DelayedMemoryDataProvider : public MemoryDataProvider {
 public:
  void RetrieveAll(const std::vector<MemoryDataType>& types,
                   base::OnceCallback<void(std::vector<MemorySearchResult>)>
                       callback) override {
    callbacks_.push_back(std::move(callback));
  }
  void CompleteNext(std::vector<MemorySearchResult> results) {
    if (!callbacks_.empty()) {
      std::move(callbacks_.front()).Run(std::move(results));
      callbacks_.erase(callbacks_.begin());
    }
  }

 private:
  std::vector<base::OnceCallback<void(std::vector<MemorySearchResult>)>>
      callbacks_;
};

class AtMemoryQueryServiceTest : public testing::Test {
 public:
  AtMemoryQueryServiceTest() = default;

 protected:
  void StubFetchContextResponse(
      personal_context::proto::AtMemoryQueryResponse response) {
    personal_context::proto::Any serialized_response;
    serialized_response.set_value(response.SerializeAsString());
    personal_context::FetchContextResult result(std::move(serialized_response));

    EXPECT_CALL(
        mock_service_,
        FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY,
                     _, _, _))
        .WillOnce(base::test::RunOnceCallback<3>(std::move(result)));
  }

  void StubFetchContextError(personal_context::ContextMemoryError error) {
    personal_context::FetchContextResult result(
        base::unexpected(std::move(error)));

    EXPECT_CALL(
        mock_service_,
        FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY,
                     _, _, _))
        .WillOnce(base::test::RunOnceCallback<3>(std::move(result)));
  }

  personal_context::proto::AtMemoryQueryResponse
  CreateQueryResponseWithSchemafulKey(
      personal_context::proto::MemoryDataType type,
      const std::string& value,
      double relevance_score = 1.0) {
    personal_context::proto::AtMemoryQueryResponse response;
    personal_context::proto::AtMemorySearchResult* result_proto =
        response.add_results();
    result_proto->set_relevance_score(relevance_score);
    personal_context::proto::Attribute* primary =
        result_proto->mutable_primary_attribute();
    primary->set_schemaful_key(type);
    primary->set_value(value);
    return response;
  }

  personal_context::proto::AtMemoryQueryResponse
  CreateQueryResponseWithSchemalessKey(const std::string& key,
                                       const std::string& value,
                                       double relevance_score = 1.0) {
    personal_context::proto::AtMemoryQueryResponse response;
    personal_context::proto::AtMemorySearchResult* result_proto =
        response.add_results();
    result_proto->set_relevance_score(relevance_score);
    personal_context::proto::Attribute* primary =
        result_proto->mutable_primary_attribute();
    primary->set_schemaless_key(key);
    primary->set_value(value);
    return response;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<personal_context::MockPersonalContextService> mock_service_;
};

// Tests that the query service returns an internal failure status after
// shutdown.
TEST_F(AtMemoryQueryServiceTest, Query_AfterShutdown) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(), &mock_service_, "en-US");

  service->Shutdown();

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kInternalFailure);
}

// Tests that the query service returns a data fetch failure immediately when
// the network is offline.
TEST_F(AtMemoryQueryServiceTest, Query_Offline) {
  net::test::ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(), &mock_service_, "en-US");

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kNoConnectionFailure);
}

// Tests that the query service returns remote results even when no local
// provider is configured.
TEST_F(AtMemoryQueryServiceTest, Query_NoLocalProviderButHasRemote) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AtMemorySearchResult* result_proto =
      response.add_results();
  personal_context::proto::Attribute* primary =
      result_proto->mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
  primary->set_value("Alice");
  result_proto->set_relevance_score(0.9);
  StubFetchContextResponse(std::move(response));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      /*data_provider=*/nullptr, &mock_service_, "en-US");

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"Alice's phone", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].value, u"Alice");
}

// Tests that the query service fetches correct local data types based on the
// `AutofillFetchPlan`.
TEST_F(AtMemoryQueryServiceTest, Query_FetchesAutofillFetchPlanTypes) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_PHONE);
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  MemorySearchResult local_phone(MemoryDataType::kPhone, u"Phone", u"123-456");
  MemorySearchResult local_name(MemoryDataType::kNameFull, u"Name",
                                u"John Doe");
  fake_data_provider->SetResults({local_phone, local_name});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"Alice's phone", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(
      fake_data_provider->last_types(),
      testing::ElementsAre(MemoryDataType::kPhone, MemoryDataType::kNameFull));
  ASSERT_EQ(result.entries.size(), 2u);
  EXPECT_EQ(result.entries[0].value, u"123-456");
  EXPECT_EQ(result.entries[1].value, u"John Doe");
}

// Tests that the query service filters local data using `filter_keywords` in
// the `AutofillFetchPlan`.
TEST_F(AtMemoryQueryServiceTest, Query_FiltersLocalDataUsingFetchPlanKeywords) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  plan->add_filter_keywords("home");

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  MemorySearchResult home_address(MemoryDataType::kAddressFull, u"Address",
                                  u"123 San Diego St Home San Diego");
  MemorySearchResult work_address(MemoryDataType::kAddressFull, u"Address",
                                  u"456 Mountain View Rd Work Mountain View");
  fake_data_provider->SetResults({home_address, work_address});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].value, u"123 San Diego St Home San Diego");
}

// Tests that local Autofill results precede remote results in the final output.
TEST_F(AtMemoryQueryServiceTest, Query_LocalResultsPrecedeRemoteResults) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  personal_context::proto::AtMemorySearchResult* remote_result =
      response.add_results();
  personal_context::proto::Attribute* primary =
      remote_result->mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
  primary->set_value("Remote Name");
  remote_result->set_relevance_score(0.9);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  MemorySearchResult local_name(MemoryDataType::kNameFull, u"Name",
                                u"Local Name");
  fake_data_provider->SetResults({local_name});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  ASSERT_EQ(result.entries.size(), 2u);
  EXPECT_EQ(result.entries[0].value, u"Local Name");
  EXPECT_EQ(result.entries[1].value, u"Remote Name");
}

// Tests that results with the most matching filter words are retained, ties for
// the highest match count keep all tied entries, and lower match entries are
// filtered out. Also tests non-ASCII case-folding.
TEST_F(AtMemoryQueryServiceTest,
       Query_WithFilterWords_HighestMatchCountAndTie) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  plan->add_filter_keywords("münchen");
  plan->add_filter_keywords("karl");

  StubFetchContextResponse(std::move(response));

  // Entry with only 1 match ("München").
  MemorySearchResult entry_a(MemoryDataType::kAddressFull, u"Address",
                             u"Hauptstraße 742, München, DE");
  entry_a.metadata_list.emplace_back(MemoryDataType::kNameFull, u"Name",
                                     u"Homer Simpson");

  // Entry with 0 matches.
  MemorySearchResult entry_b(MemoryDataType::kAddressFull, u"Address",
                             u"1st Avenue, Berlin, DE");
  entry_b.metadata_list.emplace_back(MemoryDataType::kNameFull, u"Name",
                                     u"Marge Simpson");

  // Entry with 2 matches ("Karl" in metadata, "München" in value).
  MemorySearchResult entry_c(MemoryDataType::kAddressFull, u"Address",
                             u"Hauptstraße 742, München, DE");
  entry_c.metadata_list.emplace_back(MemoryDataType::kNameFull, u"Name",
                                     u"Karl");

  // Entry with 2 matches ("Karl" in metadata, "München" in value) - tie with C.
  MemorySearchResult entry_d(MemoryDataType::kAddressFull, u"Address",
                             u"Marienplatz 100, München, DE");
  entry_d.metadata_list.emplace_back(MemoryDataType::kNameFull, u"Name",
                                     u"KARL HEINZ");

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();
  fake_data_provider->SetResults({entry_a, entry_b, entry_c, entry_d});

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "de-DE");
  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"Karl Adresse in MÜNCHEN", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ASSERT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  // Highest match count is 2 (`entry_c` and `entry_d`). Both should be
  // retained.
  EXPECT_EQ(result.entries.size(), 2u);
  EXPECT_THAT(result.entries,
              testing::UnorderedElementsAre(
                  testing::Field(&MemorySearchResult::value,
                                 u"Hauptstraße 742, München, DE"),
                  testing::Field(&MemorySearchResult::value,
                                 u"Marienplatz 100, München, DE")));
}

// Tests that the query service falls back to returning all results for the
// classified intent if none of the results match the filter words.
TEST_F(AtMemoryQueryServiceTest, Query_WithFilterWords_NoMatch_ReturnsAll) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  plan->add_filter_keywords("berlin");

  StubFetchContextResponse(std::move(response));

  MemorySearchResult entry(MemoryDataType::kAddressFull, u"Address",
                           u"123 San Diego St Home San Diego");
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();
  fake_data_provider->SetResults({entry});

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in Berlin",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"123 San Diego St Home San Diego")));
}

// Tests that the query service returns the appropriate error status when the
// personal context resolver fails.
TEST_F(AtMemoryQueryServiceTest, Query_PersonalContextResolverError) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(), &mock_service_, "en-US");

  StubFetchContextError(
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kPermissionDenied));

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kInternalFailure);
  EXPECT_TRUE(result.entries.empty());
}

// Tests that the query service does not send results for a query that has been
// superseded by a newer query.
TEST_F(AtMemoryQueryServiceTest, StaleResultsAreNotSent) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  personal_context::proto::Any serialized_response1;
  serialized_response1.set_value(response.SerializeAsString());
  personal_context::FetchContextResult result1(std::move(serialized_response1));
  auto shared_result1 = std::make_shared<personal_context::FetchContextResult>(
      std::move(result1));

  personal_context::proto::Any serialized_response2;
  serialized_response2.set_value(response.SerializeAsString());
  personal_context::FetchContextResult result2(std::move(serialized_response2));
  auto shared_result2 = std::make_shared<personal_context::FetchContextResult>(
      std::move(result2));

  EXPECT_CALL(
      mock_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, _,
                   _, _))
      .WillOnce(
          [shared_result1](
              personal_context::proto::ContextMemoryFeature feature,
              const google::protobuf::MessageLite& request_metadata,
              const personal_context::ContextMemoryRequestOptions& options,
              personal_context::FetchContextCallback callback) {
            std::move(callback).Run(std::move(*shared_result1));
          })
      .WillOnce(
          [shared_result2](
              personal_context::proto::ContextMemoryFeature feature,
              const google::protobuf::MessageLite& request_metadata,
              const personal_context::ContextMemoryRequestOptions& options,
              personal_context::FetchContextCallback callback) {
            std::move(callback).Run(std::move(*shared_result2));
          });

  auto data_provider = std::make_unique<DelayedMemoryDataProvider>();
  DelayedMemoryDataProvider* fake_data_provider = data_provider.get();
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  base::test::TestFuture<MemorySearchResults> future1;
  service->Query(u"what is my name", future1.GetRepeatingCallback());

  // Start a second query before the first one completes.
  base::test::TestFuture<MemorySearchResults> future2;
  service->Query(u"what is my address", future2.GetRepeatingCallback());

  // Complete the first query's data retrieval.
  fake_data_provider->CompleteNext({});

  // The first query's callback should NOT be called.
  EXPECT_FALSE(future1.IsReady());

  // Complete the second query's data retrieval.
  fake_data_provider->CompleteNext({});

  // The second query's callback should be called.
  ASSERT_TRUE(future2.Wait());
}

// Tests that deduplication preserves the original insertion order.
TEST_F(AtMemoryQueryServiceTest, Query_DeduplicatesResults_PreservesOrder) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"Alice");
  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"Bob");
  MemorySearchResult result3(MemoryDataType::kNameFull, u"Name",
                             u"Alice");  // duplicate of result1
  MemorySearchResult result4(MemoryDataType::kNameFull, u"Name", u"Charlie");

  fake_data_provider->SetResults({result1, result2, result3, result4});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const MemorySearchResults& result = future.Get();
  EXPECT_THAT(
      result.entries,
      testing::ElementsAre(Field(&MemorySearchResult::value, u"Alice"),
                           Field(&MemorySearchResult::value, u"Bob"),
                           Field(&MemorySearchResult::value, u"Charlie")));
}

// Tests that deduplication retains fields like confidence_score from the first
// entry and merges sources.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_RetainsFirstEntryFieldsAndMergesSources) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  EntryMetadata metadata(MemoryDataType::kAddressCity, u"City", u"San Diego");

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe",
                             /*confidence_score=*/0.9);
  result1.metadata_list.push_back(metadata);
  result1.sources.push_back(
      MemoryEntrySource(MemoryEntrySourceType::kAutofill));

  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"John Doe",
                             /*confidence_score=*/0.5);
  result2.metadata_list.push_back(metadata);
  result2.sources.push_back(MemoryEntrySource(MemoryEntrySourceType::kGmail));
  // Duplicate source shouldn't be added twice.
  result2.sources.push_back(
      MemoryEntrySource(MemoryEntrySourceType::kAutofill));

  fake_data_provider->SetResults({result1, result2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

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
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_KeepsDifferentEntries) {
  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  EntryMetadata metadata_sd(MemoryDataType::kAddressCity, u"City",
                            u"San Diego");
  EntryMetadata metadata_ny(MemoryDataType::kAddressCity, u"City", u"New York");

  // Same value, different metadata
  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  result1.metadata_list.push_back(metadata_sd);

  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"John Doe");
  result2.metadata_list.push_back(metadata_ny);

  // Different value, same metadata
  MemorySearchResult result3(MemoryDataType::kNameFull, u"Name", u"Jane Doe");
  result3.metadata_list.push_back(metadata_sd);

  // Same value and metadata, different type
  MemorySearchResult result4(MemoryDataType::kUnknown, u"Unknown", u"John Doe");
  result4.metadata_list.push_back(metadata_sd);

  fake_data_provider->SetResults({result1, result2, result3, result4});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ASSERT_EQ(result.entries.size(), 4u);
  EXPECT_EQ(result.entries[0].value, u"John Doe");
  ASSERT_EQ(result.entries[0].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[0].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[0].type, MemoryDataType::kNameFull);

  EXPECT_EQ(result.entries[1].value, u"John Doe");
  ASSERT_EQ(result.entries[1].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[1].metadata_list[0].value, u"New York");
  EXPECT_EQ(result.entries[1].type, MemoryDataType::kNameFull);

  EXPECT_EQ(result.entries[2].value, u"Jane Doe");
  ASSERT_EQ(result.entries[2].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[2].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[2].type, MemoryDataType::kNameFull);

  EXPECT_EQ(result.entries[3].value, u"John Doe");
  ASSERT_EQ(result.entries[3].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[3].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[3].type, MemoryDataType::kUnknown);
}

// Tests that the query service records the provider result count metric.
TEST_F(AtMemoryQueryServiceTest, RecordsProviderResultCountMetric) {
  base::HistogramTester histogram_tester;

  personal_context::proto::AtMemoryQueryResponse response;
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"Jane Doe");
  fake_data_provider->SetResults({result1, result2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.AtMemoryQueryService.ProviderResultCount."
      "AutofillDataProvider",
      /*sample=*/2, /*expected_bucket_count=*/1);
}

// Tests that the debug personal context mode retrieves the memory data type
// specified by the feature parameter.
TEST_F(AtMemoryQueryServiceTest,
       Query_PersonalContextDebug_CustomTypeParam_ReturnsConfiguredType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      personal_context::features::debug::kMockPersonalContextResult,
      {{personal_context::features::debug::kMockPersonalContextResultTypeParam
            .name,
        "1"}});

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), &mock_service_, "en-US");

  MemorySearchResult name_entry(MemoryDataType::kNameFull, u"Name",
                                u"Jane Doe");
  fake_data_provider->SetResults({name_entry});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries, testing::ElementsAre(Field(
                                  &MemorySearchResult::value, u"Jane Doe")));
  EXPECT_EQ(fake_data_provider->last_type(), MemoryDataType::kNameFull);
}

// Tests that the debug personal context mode returns local address suggestions.
TEST_F(AtMemoryQueryServiceTest,
       Query_PersonalContextDebug_ReturnsLocalAddressSuggestions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      personal_context::features::debug::kMockPersonalContextResult);

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), /*personal_context_service=*/nullptr,
      /*locale=*/"");

  MemorySearchResult address_entry(MemoryDataType::kAddressFull, u"Address",
                                   u"123 Main St, Anytown");
  fake_data_provider->SetResults({address_entry});

  base::test::TestFuture<MemorySearchResults> future;
  // Send an unrelated query to verify it still returns local address
  // suggestions.
  service->Query(u"random query string 12345", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries,
              testing::ElementsAre(
                  Field(&MemorySearchResult::value, u"123 Main St, Anytown")));
  EXPECT_EQ(fake_data_provider->last_type(), MemoryDataType::kAddressFull);
}

// Tests that the query service correctly identifies and marks SPII data types
// as obfuscated, while leaving non-SPII data types set to unobfuscated.
TEST_F(AtMemoryQueryServiceTest, Query_SetsIsObfuscated) {
  // Prepare a fake server response.
  personal_context::proto::AtMemoryQueryResponse response;
  response.set_query_classification(
      personal_context::proto::AtMemoryQueryResponse::
          QUERY_CLASSIFICATION_AT_MEMORY);

  // 1. Non-SPII: Full Name
  personal_context::proto::AtMemorySearchResult* result1 =
      response.add_results();
  result1->mutable_primary_attribute()->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
  result1->mutable_primary_attribute()->set_value("John Doe");

  // 2. SPII: Credit Card Number
  personal_context::proto::AtMemorySearchResult* result2 =
      response.add_results();
  result2->mutable_primary_attribute()->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NUMBER);
  result2->mutable_primary_attribute()->set_value("1111222233334444");

  StubFetchContextResponse(std::move(response));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(), &mock_service_, "en-US");

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"some query", future.GetRepeatingCallback());

  const MemorySearchResults& search_results = future.Get();
  EXPECT_THAT(
      search_results.entries,
      UnorderedElementsAre(
          AllOf(Field(&MemorySearchResult::type, MemoryDataType::kNameFull),
                Field(&MemorySearchResult::is_obfuscated, false)),
          AllOf(Field(&MemorySearchResult::type,
                      MemoryDataType::kCreditCardNumber),
                Field(&MemorySearchResult::is_obfuscated, true))));
}

}  // namespace

}  // namespace accessibility_annotator
