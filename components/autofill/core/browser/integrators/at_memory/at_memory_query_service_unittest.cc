// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using ::base::test::ErrorIs;
using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::Values;

class FakeMemoryDataProvider : public AutofillDataProvider {
 public:
  FakeMemoryDataProvider() : AutofillDataProvider(nullptr, nullptr) {}
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

class DelayedMemoryDataProvider : public AutofillDataProvider {
 public:
  DelayedMemoryDataProvider() : AutofillDataProvider(nullptr, nullptr) {}
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

class AtMemoryQueryServiceTest : public testing::Test,
                                 public WithTestAutofillClientDriverManager<> {
 public:
  AtMemoryQueryServiceTest() { InitAutofillClient(); }

 protected:
  void StubFetchContextResponse(
      personal_context::proto::AtMemoryQueryResponse response) {
    personal_context::proto::Any serialized_response;
    serialized_response.set_value(response.SerializeAsString());
    personal_context::FetchContextResult result(std::move(serialized_response),
                                                "server request id");

    EXPECT_CALL(
        mock_service(),
        FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY,
                     _, _, _))
        .WillOnce(RunOnceCallback<3>(std::move(result)));
  }

  void StubFetchContextError(personal_context::ContextMemoryError error) {
    personal_context::FetchContextResult result(
        base::unexpected(std::move(error)));

    EXPECT_CALL(
        mock_service(),
        FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY,
                     _, _, _))
        .WillOnce(RunOnceCallback<3>(std::move(result)));
  }

  personal_context::proto::AtMemoryQueryResponse CreateQueryResponse() {
    personal_context::proto::AtMemoryQueryResponse response;
    response.set_query_classification(
        personal_context::proto::AtMemoryQueryResponse::
            QUERY_CLASSIFICATION_AT_MEMORY);
    return response;
  }

  personal_context::proto::AtMemoryQueryResponse
  CreateQueryResponseWithSchemafulKey(
      personal_context::proto::MemoryDataType type,
      const std::string& value,
      double relevance_score = 1.0) {
    personal_context::proto::AtMemoryQueryResponse response =
        CreateQueryResponse();
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
    personal_context::proto::AtMemoryQueryResponse response =
        CreateQueryResponse();
    personal_context::proto::AtMemorySearchResult* result_proto =
        response.add_results();
    result_proto->set_relevance_score(relevance_score);
    personal_context::proto::Attribute* primary =
        result_proto->mutable_primary_attribute();
    primary->set_schemaless_key(key);
    primary->set_value(value);
    return response;
  }

  MemorySearchResults RunDeduplicationQueryWithLocalResults(
      const std::vector<MemorySearchResult>& local_results) {
    personal_context::proto::AtMemoryQueryResponse response =
        CreateQueryResponse();
    personal_context::proto::AutofillFetchPlan* plan =
        response.mutable_autofill_fetch_plan();
    plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
    StubFetchContextResponse(std::move(response));

    auto data_provider = std::make_unique<FakeMemoryDataProvider>();
    data_provider->SetResults(local_results);

    auto service = std::make_unique<AtMemoryQueryService>(
        std::move(data_provider), &mock_service_, "en-US");

    base::test::TestFuture<MemorySearchResults> future;
    service->Query(u"what is my name", GURL("https://example.com"),
                   u"Page Title", future.GetRepeatingCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  personal_context::MockPersonalContextService& mock_service() {
    return mock_service_;
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<personal_context::MockPersonalContextService> mock_service_;
};

// Tests that the query service returns an internal failure status after
// shutdown.
TEST_F(AtMemoryQueryServiceTest, Query_AfterShutdown) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  service->Shutdown();

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

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
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kNoConnectionFailure);
}

// Tests that the query service returns remote results even when no local
// provider is configured.
TEST_F(AtMemoryQueryServiceTest, Query_NoLocalProviderButHasRemote) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
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
      /*data_provider=*/nullptr, &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"Alice's phone", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].value, u"Alice");
  EXPECT_EQ(result.entries[0].remote_response_index, 0);
  EXPECT_EQ(result.server_request_id, "server request id");
}

// Tests that the query service fetches correct local data types based on the
// `AutofillFetchPlan`.
TEST_F(AtMemoryQueryServiceTest, Query_FetchesAutofillFetchPlanTypes) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_PHONE);
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  MemorySearchResult local_phone(MemoryDataType::kPhone, u"Phone", u"123-456");
  MemorySearchResult local_name(MemoryDataType::kNameFull, u"Name",
                                u"John Doe");
  fake_data_provider->SetResults({local_phone, local_name});

  TestFuture<MemorySearchResults> future;
  service->Query(u"Alice's phone", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(fake_data_provider->last_types(),
              ElementsAre(MemoryDataType::kPhone, MemoryDataType::kNameFull));
  ASSERT_EQ(result.entries.size(), 2u);
  EXPECT_EQ(result.entries[0].value, u"123-456");
  EXPECT_EQ(result.entries[1].value, u"John Doe");
  EXPECT_EQ(result.server_request_id, "server request id");
}


// Tests that rationalization handles multiple groups and deduplicates types
// while preserving order.
TEST_F(AtMemoryQueryServiceTest,
       Query_RationalizesAutofillFetchPlanTypes_MultipleGroupsAndDuplicates) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_VEHICLE);
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MAKE);
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NAME);
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_FULL);
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_PHONE);
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_PHONE);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"my info", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(
      fake_data_provider->last_types(),
      ElementsAre(MemoryDataType::kVehiclePlateNumber,
                  MemoryDataType::kPassportNumber, MemoryDataType::kPhone));
}

// Tests that `MemoryDataType::kUnknown` is filtered out of AutofillFetchPlan
// types.
TEST_F(AtMemoryQueryServiceTest,
       Query_RationalizesAutofillFetchPlanTypes_FiltersUnknownType) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_UNSPECIFIED);
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_PHONE);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"phone number", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(fake_data_provider->last_types(),
              ElementsAre(MemoryDataType::kPhone));
}


// Tests that the query service filters local data using `filter_keywords` in
// the `AutofillFetchPlan`.
TEST_F(AtMemoryQueryServiceTest, Query_FiltersLocalDataUsingFetchPlanKeywords) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  plan->add_filter_keywords("home");

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  MemorySearchResult home_address(MemoryDataType::kAddressFull, u"Address",
                                  u"123 San Diego St Home San Diego");
  MemorySearchResult work_address(MemoryDataType::kAddressFull, u"Address",
                                  u"456 Mountain View Rd Work Mountain View");
  fake_data_provider->SetResults({home_address, work_address});

  TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address", GURL("https://example.com"),
                 u"Page Title", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].value, u"123 San Diego St Home San Diego");
}

// Tests that local Autofill results precede remote results in the final output.
TEST_F(AtMemoryQueryServiceTest, Query_LocalResultsPrecedeRemoteResults) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
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
      std::move(data_provider), &mock_service(), "en-US");

  MemorySearchResult local_name(MemoryDataType::kNameFull, u"Name",
                                u"Local Name");
  fake_data_provider->SetResults({local_name});

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  ASSERT_EQ(result.entries.size(), 2u);
  EXPECT_EQ(result.entries[0].value, u"Local Name");
  EXPECT_EQ(result.entries[0].remote_response_index, std::nullopt);
  EXPECT_EQ(result.entries[1].value, u"Remote Name");
  EXPECT_EQ(result.entries[1].remote_response_index, 0);
}

// Tests that results with the most matching filter words are retained, ties for
// the highest match count keep all tied entries, and lower match entries are
// filtered out. Also tests non-ASCII case-folding.
TEST_F(AtMemoryQueryServiceTest,
       Query_WithFilterWords_HighestMatchCountAndTie) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
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
      std::move(data_provider), &mock_service(), "de-DE");
  TestFuture<MemorySearchResults> future;
  service->Query(u"Karl Adresse in MÜNCHEN", GURL("https://example.com"),
                 u"Page Title", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ASSERT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  // Highest match count is 2 (`entry_c` and `entry_d`). Both should be
  // retained.
  EXPECT_EQ(result.entries.size(), 2u);
  EXPECT_THAT(
      result.entries,
      UnorderedElementsAre(
          Field(&MemorySearchResult::value, u"Hauptstraße 742, München, DE"),
          Field(&MemorySearchResult::value, u"Marienplatz 100, München, DE")));
}

// Tests that the query service returns no results if filter keywords are
// provided and no entries match.
TEST_F(AtMemoryQueryServiceTest, Query_WithFilterWords_NoMatch_ReturnsEmpty) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
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
      std::move(data_provider), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in Berlin",
                 GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_TRUE(result.entries.empty());
}

// Tests that the query service returns the appropriate error status when the
// personal context resolver fails.
TEST_F(AtMemoryQueryServiceTest, Query_PersonalContextResolverError) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  StubFetchContextError(
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kPermissionDenied));

  TestFuture<MemorySearchResults> future;
  service->Query(u"random query", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

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
  response.set_query_classification(
      personal_context::proto::AtMemoryQueryResponse::
          QUERY_CLASSIFICATION_AT_MEMORY);

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
      mock_service(),
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
      std::move(data_provider), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future1;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future1.GetRepeatingCallback());

  // Start a second query before the first one completes.
  TestFuture<MemorySearchResults> future2;
  service->Query(u"what is my address", GURL("https://example.com"),
                 u"Page Title", future2.GetRepeatingCallback());

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
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"Alice");
  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"Bob");
  MemorySearchResult result3(MemoryDataType::kNameFull, u"Name",
                             u"Alice");  // duplicate of result1
  MemorySearchResult result4(MemoryDataType::kNameFull, u"Name", u"Charlie");

  fake_data_provider->SetResults({result1, result2, result3, result4});

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const MemorySearchResults& result = future.Get();
  EXPECT_THAT(result.entries,
              ElementsAre(Field(&MemorySearchResult::value, u"Alice"),
                          Field(&MemorySearchResult::value, u"Bob"),
                          Field(&MemorySearchResult::value, u"Charlie")));
}

// Tests that deduplication is case-insensitive for values.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_CaseInsensitiveValue) {
  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"john doe");

  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 1u);
}

// Tests that deduplication is case-insensitive for metadata.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_CaseInsensitiveMetadata) {
  EntryMetadata sd_meta1(MemoryDataType::kAddressCity, u"City", u"San Diego");
  EntryMetadata sd_meta2(MemoryDataType::kAddressCity, u"City", u"san diego");

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  result1.metadata_list.push_back(sd_meta1);

  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"John Doe");
  result2.metadata_list.push_back(sd_meta2);

  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 1u);
}

// Tests that deduplication is case-insensitive for merge constraints.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_CaseInsensitiveMergeConstraints) {
  MemorySearchResult result1(MemoryDataType::kPassportName, u"Name",
                             u"John Doe");
  result1.metadata_list.emplace_back(MemoryDataType::kPassportNumber,
                                     u"Passport Number", u"abc12");

  MemorySearchResult result2(MemoryDataType::kPassportName, u"Name",
                             u"John Doe");
  result2.metadata_list.emplace_back(MemoryDataType::kPassportNumber,
                                     u"Passport Number", u"ABC12");

  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 1u);
}

// Tests that deduplication retains fields like confidence_score from the first
// entry.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_RetainsFirstEntryFields) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  EntryMetadata metadata(MemoryDataType::kAddressCity, u"City", u"San Diego");

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe",
                             /*confidence_score=*/0.9);
  result1.is_local = true;
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

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].value, u"John Doe");
  EXPECT_DOUBLE_EQ(result.entries[0].confidence_score, 0.9);
  EXPECT_TRUE(result.entries[0].is_local);
  ASSERT_EQ(result.entries[0].sources.size(), 1u);
  EXPECT_EQ(result.entries[0].sources[0].type,
            MemoryEntrySourceType::kAutofill);
}

// Tests that entries with different values or metadata lists are both retained.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_KeepsDifferentEntries) {
  EntryMetadata sd_meta(MemoryDataType::kAddressCity, u"City", u"San Diego");
  EntryMetadata ny_meta(MemoryDataType::kAddressCity, u"City", u"New York");
  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  result1.metadata_list.push_back(sd_meta);

  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"John Doe");
  result2.metadata_list.push_back(ny_meta);

  MemorySearchResult result3(MemoryDataType::kNameFull, u"Name", u"Jane Doe");
  result3.metadata_list.push_back(sd_meta);

  // Same value and metadata, different type
  MemorySearchResult result4(MemoryDataType::kUnknown, u"Unknown", u"John Doe");
  result4.metadata_list.push_back(sd_meta);

  const MemorySearchResults& result = RunDeduplicationQueryWithLocalResults(
      {result1, result2, result3, result4});
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

// Tests that deduplication prefers explicitly saved local Autofill results.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_PrefersLocalAutofill) {
  MemorySearchResult remote_result(MemoryDataType::kNameFull, u"Name",
                                   u"John Doe", /*confidence_score=*/0.9);
  remote_result.is_local = false;

  MemorySearchResult local_result(MemoryDataType::kNameFull, u"Name",
                                  u"John Doe", /*confidence_score=*/0.5);
  local_result.is_local = true;

  // Insert remote first to test tiebreaker overriding the first entry.
  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({remote_result, local_result});
  EXPECT_THAT(result.entries, testing::ElementsAre(local_result));
}

// Tests that deduplication prefers results with more non-empty metadata fields.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_PrefersMoreMetadata) {
  MemorySearchResult less_meta(MemoryDataType::kNameFull, u"Name", u"John Doe",
                               /*confidence_score=*/0.9);
  less_meta.metadata_list.emplace_back(MemoryDataType::kAddressCity, u"City",
                                       u"San Diego");

  MemorySearchResult more_meta(MemoryDataType::kNameFull, u"Name", u"John Doe",
                               /*confidence_score=*/0.5);
  more_meta.metadata_list.emplace_back(MemoryDataType::kAddressCity, u"City",
                                       u"San Diego");
  more_meta.metadata_list.emplace_back(MemoryDataType::kAddressState, u"State",
                                       u"CA");

  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({less_meta, more_meta});
  EXPECT_THAT(result.entries, testing::ElementsAre(more_meta));
}

// Tests that deduplication prefers results sourced from Autofill.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_PrefersAutofillSource) {
  MemorySearchResult gmail_result(MemoryDataType::kNameFull, u"Name",
                                  u"John Doe", /*confidence_score=*/0.9);
  gmail_result.sources.emplace_back(MemoryEntrySourceType::kGmail);

  MemorySearchResult autofill_result(MemoryDataType::kNameFull, u"Name",
                                     u"John Doe", /*confidence_score=*/0.5);
  autofill_result.sources.emplace_back(MemoryEntrySourceType::kAutofill);

  // We check the entry was replaced based on confidence score changing to 0.5.
  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({gmail_result, autofill_result});
  EXPECT_THAT(result.entries, testing::ElementsAre(autofill_result));
}

// Tests that deduplication for Autofill AI entities is determined by merge
// constraints (e.g. Passport Number), ignoring other contradicting metadata.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_MergeConstraintsSatisfied) {
  MemorySearchResult result1(MemoryDataType::kPassportNumber,
                             u"Passport Number", u"12345");
  result1.metadata_list.emplace_back(MemoryDataType::kPassportName, u"Name",
                                     u"John Doe");

  MemorySearchResult result2(MemoryDataType::kPassportNumber,
                             u"Passport Number", u"12345");
  result2.metadata_list.emplace_back(MemoryDataType::kPassportName, u"Name",
                                     u"Jane Doe");

  // The merge constraint for Passport is `kPassportNumber`. Since both results
  // have the same Passport Number (12345), they are considered duplicates
  // despite the contradicting `kPassportName`.
  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 1u);
}

// Tests that deduplication works when the local data is obfuscated with dots
// and the remote one is a raw suffix.
TEST_F(AtMemoryQueryServiceTest, Query_DeduplicatesResults_ObfuscatedValues) {
  // Local, obfuscated result.
  std::u16string raw_value = u"DL123456789012";
  std::u16string obfuscated_value = GetObfuscatedValue(raw_value, 4);

  MemorySearchResult result1(MemoryDataType::kDriversLicenseNumber,
                             u"Driver's license number", obfuscated_value);
  result1.is_local = true;
  result1.metadata_list.emplace_back(MemoryDataType::kDriversLicenseName,
                                     u"Name", u"John Doe");

  // Remote result with last 4 digits.
  MemorySearchResult result2(MemoryDataType::kDriversLicenseNumber,
                             u"Driver's license number", u"9012");
  result2.is_local = false;
  result2.metadata_list.emplace_back(MemoryDataType::kDriversLicenseName,
                                     u"Name", u"John Doe");

  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 1u);
}

// Tests that Autofill AI entities are not deduplicated if their merge
// constraints are not satisfied, even if their main values match.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_MergeConstraintsNotSatisfied_NotDeduplicated) {
  MemorySearchResult result1(MemoryDataType::kPassportName, u"Name",
                             u"John Doe");
  result1.metadata_list.emplace_back(MemoryDataType::kPassportNumber,
                                     u"Passport Number", u"888");

  MemorySearchResult result2(MemoryDataType::kPassportName, u"Name",
                             u"John Doe");
  result2.metadata_list.emplace_back(MemoryDataType::kPassportNumber,
                                     u"Passport Number", u"999");

  // Their primary values match ("John Doe"), but their merge constraints
  // (`kPassportNumber`) do not match (888 vs 999). Therefore, they correspond
  // to different passport entities and are not deduplicated.
  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 2u);
}

// Tests that Autofill AI entities are deduplicated via fallback
// non-contradicting metadata logic when no merge constraints are present or
// satisfied.
TEST_F(
    AtMemoryQueryServiceTest,
    Query_DeduplicatesResults_MergeConstraintsMissing_FallsBackToMetadataMatching) {
  MemorySearchResult result1(MemoryDataType::kFlightReservationFlightNumber,
                             u"Flight Number", u"FL123");
  result1.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationDepartureDate, u"Departure Date",
      u"2026-01-01");
  result1.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationDepartureAirport, u"Departure Airport",
      u"ABC");
  result1.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationArrivalAirport, u"Arrival Airport",
      u"XYZ");

  MemorySearchResult result2(MemoryDataType::kFlightReservationFlightNumber,
                             u"Flight Number", u"FL123");
  result2.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationDepartureDate, u"Departure Date",
      u"2026-01-01");
  result2.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationDepartureAirport, u"Departure Airport",
      u"ABC");
  result2.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationArrivalAirport, u"Arrival Airport",
      u"XYZ");

  // Neither result has merge constraint attributes
  // (`kFlightReservationConfirmationCode` or `kFlightReservationTicketNumber`).
  // Since primary values and secondary metadata match without contradiction,
  // fallback logic deduplicates them.
  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 1u);
}

// Tests that the query service records the provider result count metric.
TEST_F(AtMemoryQueryServiceTest, RecordsProviderResultCountMetric) {
  base::HistogramTester histogram_tester;

  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
  personal_context::proto::AutofillFetchPlan* plan =
      response.mutable_autofill_fetch_plan();
  plan->add_data_types(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"Jane Doe");
  fake_data_provider->SetResults({result1, result2});

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

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
      std::move(data_provider), &mock_service(), "en-US");

  MemorySearchResult name_entry(MemoryDataType::kNameFull, u"Name",
                                u"Jane Doe");
  fake_data_provider->SetResults({name_entry});

  TestFuture<MemorySearchResults> future;
  service->Query(u"random query", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries,
              ElementsAre(Field(&MemorySearchResult::value, u"Jane Doe")));
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
      std::move(data_provider), /*personal_context_service=*/nullptr,
      /*locale=*/"");

  MemorySearchResult address_entry(MemoryDataType::kAddressFull, u"Address",
                                   u"123 Main St, Anytown");
  fake_data_provider->SetResults({address_entry});

  TestFuture<MemorySearchResults> future;
  // Send an unrelated query to verify it still returns local address
  // suggestions.
  service->Query(u"random query string 12345", GURL("https://example.com"),
                 u"Page Title", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries, ElementsAre(Field(&MemorySearchResult::value,
                                                u"123 Main St, Anytown")));
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
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"some query", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

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

// Tests that Autofill results are presented before remote results for
// non-dynamic types, and Autofill results are sorted by ranking score
// descending.
TEST_F(AtMemoryQueryServiceTest,
       Query_Ranking_AutofillPrioritizedForNonDynamicTypes) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponseWithSchemafulKey(
          personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL, "Remote Name");
  response.mutable_autofill_fetch_plan()->add_data_types(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  MemorySearchResult local_b(MemoryDataType::kNameFull, u"Name",
                             u"Local Name B", /*confidence_score=*/0.5);
  MemorySearchResult local_a(MemoryDataType::kNameFull, u"Name",
                             u"Local Name A", /*confidence_score=*/0.9);
  fake_data_provider->SetResults({local_b, local_a});

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const MemorySearchResults& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries,
              ElementsAre(Field(&MemorySearchResult::value, u"Local Name A"),
                          Field(&MemorySearchResult::value, u"Local Name B"),
                          Field(&MemorySearchResult::value, u"Remote Name")));
}

// Tests that remote results are presented before Autofill results when all
// results are dynamic transaction types.
TEST_F(AtMemoryQueryServiceTest,
       Query_Ranking_RemotePrioritizedForDynamicTransactionTypes) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponseWithSchemafulKey(
          personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER,
          "Remote 1Z12345");
  response.mutable_autofill_fetch_plan()->add_data_types(
      personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  MemorySearchResult local_shipment(MemoryDataType::kShipmentTrackingNumber,
                                    u"Tracking", u"Local 1Z67890",
                                    /*confidence_score=*/0.8);
  fake_data_provider->SetResults({local_shipment});

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"tracking number", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const MemorySearchResults& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries,
              ElementsAre(Field(&MemorySearchResult::value, u"Remote 1Z12345"),
                          Field(&MemorySearchResult::value, u"Local 1Z67890")));
}

// Tests that Autofill results are prioritized when mixed types contain at
// least one non-dynamic transaction type.
TEST_F(AtMemoryQueryServiceTest,
       Query_Ranking_MixedTypesNotAllDynamic_AutofillPrioritized) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponseWithSchemafulKey(
          personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER,
          "Remote 1Z12345");
  response.mutable_autofill_fetch_plan()->add_data_types(
      personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  MemorySearchResult local_address(MemoryDataType::kAddressFull, u"Address",
                                   u"123 Main St", /*confidence_score=*/0.7);
  fake_data_provider->SetResults({local_address});

  auto service = std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), &mock_service(), "en-US");

  TestFuture<MemorySearchResults> future;
  service->Query(u"where is my package", GURL("https://example.com"),
                 u"Page Title", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const MemorySearchResults& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(
      result.entries,
      ElementsAre(Field(&MemorySearchResult::value, u"123 Main St"),
                  Field(&MemorySearchResult::value, u"Remote 1Z12345")));
}

struct QueryClassificationTestCase {
  personal_context::proto::AtMemoryQueryResponse::QueryClassification
      classification;
  MemorySearchStatus expected_status;
};

class AtMemoryQueryServiceClassificationTest
    : public AtMemoryQueryServiceTest,
      public ::testing::WithParamInterface<QueryClassificationTestCase> {};

// Verifies that each query classification is correctly mapped to a search
// status.
TEST_P(AtMemoryQueryServiceClassificationTest, MapQueryClassificationToStatus) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  personal_context::proto::AtMemoryQueryResponse response;
  response.set_query_classification(GetParam().classification);
  StubFetchContextResponse(std::move(response));

  TestFuture<MemorySearchResults> future;
  service->Query(u"query", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const MemorySearchResults& result = future.Get();
  EXPECT_EQ(result.status, GetParam().expected_status);
  EXPECT_TRUE(result.entries.empty());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AtMemoryQueryServiceClassificationTest,
    ::testing::ValuesIn(std::vector<QueryClassificationTestCase>{
        {personal_context::proto::AtMemoryQueryResponse::
             QUERY_CLASSIFICATION_UNSPECIFIED,
         MemorySearchStatus::kInternalFailure},
        {personal_context::proto::AtMemoryQueryResponse::
             QUERY_CLASSIFICATION_AT_MEMORY,
         MemorySearchStatus::kFinalResponseSuccess},
        {personal_context::proto::AtMemoryQueryResponse::
             QUERY_CLASSIFICATION_UNSUPPORTED,
         MemorySearchStatus::kUnsupportedQuery},
        {personal_context::proto::AtMemoryQueryResponse::
             QUERY_CLASSIFICATION_SENSITIVE,
         MemorySearchStatus::kUnsupportedQuery},
        {personal_context::proto::AtMemoryQueryResponse::
             QUERY_CLASSIFICATION_RECITATION,
         MemorySearchStatus::kUnsupportedQuery}}));

// Tests that the query service uses the timeout specified by the feature
// parameter.
TEST_F(AtMemoryQueryServiceTest, Query_UsesTimeoutFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAtMemory,
      {{features::kAutofillAtMemoryRequestTimeout.name, "10s"}});

  EXPECT_CALL(
      mock_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, _,
          Field(&personal_context::ContextMemoryRequestOptions::request_timeout,
                base::Seconds(10)),
          _));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 base::DoNothing());
}

TEST_F(AtMemoryQueryServiceTest, Query_PopulatesUrlAndTitle) {
  EXPECT_CALL(
      mock_service(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, _,
                   _, _))
      .WillOnce([](personal_context::proto::ContextMemoryFeature feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const personal_context::ContextMemoryRequestOptions& options,
                   personal_context::FetchContextCallback callback) {
        const auto* at_memory_request =
            static_cast<const personal_context::proto::AtMemoryQueryRequest*>(
                &request_metadata);
        EXPECT_EQ(at_memory_request->input_query(), "Alice");
        EXPECT_EQ(at_memory_request->url(), "https://example.com/");
        EXPECT_EQ(at_memory_request->title(), "Example Title");
      });

  auto service = std::make_unique<AtMemoryQueryService>(
      /*data_provider=*/nullptr, &mock_service(), "en-US");

  service->Query(u"Alice", GURL("https://example.com/"), u"Example Title",
                 base::DoNothing());
}

// Tests that when there is no device authenticator,
// `AuthenticateAndFetchPiiEntity(..)` returns a `kReauthFailed` error and does
// not fetch from `PersonalContextService`.
TEST_F(AtMemoryQueryServiceTest,
       AuthenticateAndFetchPiiEntity_NoAuthenticator) {
  autofill_client().set_device_authenticator(nullptr);
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  EXPECT_CALL(mock_service(), FetchPiiEntities).Times(0);

  TestFuture<AtMemoryQueryService::SpiiRetrievalResult> future;
  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message", u"1234",
      MemoryDataType::kPassportNumber, {}, future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      ErrorIs(AtMemoryQueryService::SpiiRetrievalFailureReason::kReauthFailed));
}

// Tests that when `CanAuthenticateWithBiometricOrScreenLock()` returns false,
// `AuthenticateAndFetchPiiEntity(..)` returns a `kReauthFailed` error and does
// not trigger authentication or fetch from `PersonalContextService`.
TEST_F(AtMemoryQueryServiceTest,
       AuthenticateAndFetchPiiEntity_CannotAuthenticate) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, CanAuthenticateWithBiometricOrScreenLock())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage).Times(0);

  autofill_client().set_device_authenticator(std::move(mock_authenticator));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  EXPECT_CALL(mock_service(), FetchPiiEntities).Times(0);

  TestFuture<AtMemoryQueryService::SpiiRetrievalResult> future;
  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message", u"1234",
      MemoryDataType::kPassportNumber, {}, future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      ErrorIs(AtMemoryQueryService::SpiiRetrievalFailureReason::kReauthFailed));
}

// Tests that when authentication fails, `AuthenticateAndFetchPiiEntity(..)`
// returns a `kReauthFailed` error and does not fetch from
// `PersonalContextService`.
TEST_F(AtMemoryQueryServiceTest, AuthenticateAndFetchPiiEntity_AuthFails) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, CanAuthenticateWithBiometricOrScreenLock())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));

  autofill_client().set_device_authenticator(std::move(mock_authenticator));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  EXPECT_CALL(mock_service(), FetchPiiEntities).Times(0);

  TestFuture<AtMemoryQueryService::SpiiRetrievalResult> future;
  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message", u"1234",
      MemoryDataType::kPassportNumber, {}, future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      ErrorIs(AtMemoryQueryService::SpiiRetrievalFailureReason::kReauthFailed));
}

// Tests that when `PersonalContextService` returns a fetch error,
// `AuthenticateAndFetchPiiEntity(..)` returns a `kFetchFailed` error.
TEST_F(AtMemoryQueryServiceTest, AuthenticateAndFetchPiiEntity_FetchFails) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, CanAuthenticateWithBiometricOrScreenLock())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

  autofill_client().set_device_authenticator(std::move(mock_authenticator));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  personal_context::FetchPiiEntitiesResult result(
      base::unexpected(personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kGenericFailure)));

  EXPECT_CALL(mock_service(), FetchPiiEntities)
      .WillOnce(RunOnceCallback<2>(std::move(result)));

  TestFuture<AtMemoryQueryService::SpiiRetrievalResult> future;
  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message", u"1234",
      MemoryDataType::kPassportNumber, {}, future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      ErrorIs(AtMemoryQueryService::SpiiRetrievalFailureReason::kFetchFailed));
}

// Tests that when `PersonalContextService` returns an entity that fails
// conversion, `AuthenticateAndFetchPiiEntity(..)` returns a `kParseFailed`
// error.
TEST_F(AtMemoryQueryServiceTest, AuthenticateAndFetchPiiEntity_ParseFails) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, CanAuthenticateWithBiometricOrScreenLock())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

  autofill_client().set_device_authenticator(std::move(mock_authenticator));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  personal_context::proto::FetchPiiEntitiesResponse response;
  personal_context::proto::Entity* entity = response.add_entities();
  // Set an unsupported entity case to trigger std::nullopt conversion safely.
  entity->mutable_sensitive_pii_presence();

  personal_context::FetchPiiEntitiesResult result(std::move(response));

  EXPECT_CALL(mock_service(), FetchPiiEntities)
      .WillOnce(RunOnceCallback<2>(std::move(result)));

  TestFuture<AtMemoryQueryService::SpiiRetrievalResult> future;
  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message", u"1234",
      MemoryDataType::kPassportNumber, {}, future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      ErrorIs(AtMemoryQueryService::SpiiRetrievalFailureReason::kParseFailed));
}

// Tests a successful end-to-end unmasking flow: authenticates the user,
// fetches from `PersonalContextService`, and returns the raw unmasked value
// string.
TEST_F(AtMemoryQueryServiceTest, AuthenticateAndFetchPiiEntity_Success) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, CanAuthenticateWithBiometricOrScreenLock())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

  autofill_client().set_device_authenticator(std::move(mock_authenticator));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  personal_context::proto::FetchPiiEntitiesResponse response;
  personal_context::proto::Entity* entity = response.add_entities();
  entity->mutable_passport()->set_number("987654321");
  entity->mutable_passport()->set_name("John Doe");

  personal_context::FetchPiiEntitiesResult result(std::move(response));

  EntryMetadata metadata(MemoryDataType::kPassportExpirationDate,
                         u"Expiration Date", u"2030-01-01");

  EXPECT_CALL(mock_service(), FetchPiiEntities)
      .WillOnce(RunOnceCallback<2>(std::move(result)));

  TestFuture<AtMemoryQueryService::SpiiRetrievalResult> future;
  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message", u"4321",
      MemoryDataType::kPassportNumber, {{metadata}}, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), u"987654321");
}

// Tests that calling `AuthenticateAndFetchPiiEntity(..)` when the network is
// offline returns a `kNoConnection` error immediately without authentication or
// calling the service.
TEST_F(AtMemoryQueryServiceTest, AuthenticateAndFetchPiiEntity_Offline) {
  net::test::ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  EXPECT_CALL(mock_service(), FetchPiiEntities).Times(0);

  TestFuture<AtMemoryQueryService::SpiiRetrievalResult> future;
  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message", u"1234",
      MemoryDataType::kPassportNumber, {}, future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      ErrorIs(AtMemoryQueryService::SpiiRetrievalFailureReason::kNoConnection));
}

// Tests that calling `AuthenticateAndFetchPiiEntity(..)` while authentication
// is already in progress returns `kReauthInProgress` immediately and doesn't
// trigger a new authentication request.
TEST_F(AtMemoryQueryServiceTest, AuthenticateAndFetchPiiEntity_AuthInProgress) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, CanAuthenticateWithBiometricOrScreenLock())
      .WillOnce(Return(true));
  // AuthenticateWithMessage is called but we DO NOT run its callback
  // immediately, leaving authentication in progress.
  base::OnceCallback<void(bool)> first_auth_callback;
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce([&](const std::u16string& message,
                    base::OnceCallback<void(bool)> callback) {
        first_auth_callback = std::move(callback);
      });

  autofill_client().set_device_authenticator(std::move(mock_authenticator));

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US");

  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message", u"1234",
      MemoryDataType::kPassportNumber, {}, base::DoNothing());

  // Second call should return `kReauthInProgress` immediately.
  TestFuture<AtMemoryQueryService::SpiiRetrievalResult> second_future;
  service->AuthenticateAndFetchPiiEntity(
      autofill_client(), u"auth message 2", u"1234",
      MemoryDataType::kPassportNumber, {}, second_future.GetCallback());

  EXPECT_THAT(
      second_future.Get(),
      ErrorIs(
          AtMemoryQueryService::SpiiRetrievalFailureReason::kReauthInProgress));
}

// Tests that secondary metadata attributes are reordered by uniqueness across
// all returned suggestions (more unique values for a given attribute type
// first).
struct ReorderMetadataTestCase {
  // Input metadata entries for each search result.
  std::vector<std::vector<EntryMetadata>> input_metadata_per_result;
  // Expected metadata entries for each search result after uniqueness sorting.
  std::vector<std::vector<EntryMetadata>> expected_metadata_per_result;
};

class AtMemoryQueryServiceReorderMetadataTest
    : public AtMemoryQueryServiceTest,
      public ::testing::WithParamInterface<ReorderMetadataTestCase> {};

// Tests that secondary metadata attributes in query search results are
// reordered by uniqueness across all suggestions (more unique values of the
// same type first), preserving original relative provider order when frequency
// scores tie.
TEST_P(AtMemoryQueryServiceReorderMetadataTest, ReordersSecondaryMetadata) {
  personal_context::proto::AtMemoryQueryResponse response =
      CreateQueryResponse();
  response.mutable_autofill_fetch_plan()->add_data_types(
      personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  StubFetchContextResponse(std::move(response));

  // Build input search results with metadata based on test parameters.
  int id = 0;
  std::vector<MemorySearchResult> input_results = base::ToVector(
      GetParam().input_metadata_per_result,
      [&id](const std::vector<EntryMetadata>& metadata_list) {
        MemorySearchResult result(
            MemoryDataType::kAddressFull, u"Address",
            base::StrCat({u"Address ", base::NumberToString16(id++)}),
            /*confidence_score=*/0.9);
        result.metadata_list = metadata_list;
        return result;
      });

  std::unique_ptr<FakeMemoryDataProvider> data_provider =
      std::make_unique<FakeMemoryDataProvider>();
  data_provider->SetResults(std::move(input_results));

  std::unique_ptr<AtMemoryQueryService> service =
      std::make_unique<AtMemoryQueryService>(std::move(data_provider),
                                             &mock_service(), "en-US");

  // Execute query and wait for search results.
  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"addresses", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());
  const MemorySearchResults& search_results = future.Get();

  // Construct matchers that will verify secondary metadata ordering matches
  // expectations for each result.
  std::vector<Matcher<const MemorySearchResult&>> entry_matchers =
      base::ToVector(
          GetParam().expected_metadata_per_result,
          [](const std::vector<EntryMetadata>& expected_list)
              -> Matcher<const MemorySearchResult&> {
            return Field(
                &MemorySearchResult::metadata_list,
                ElementsAreArray(base::ToVector(
                    expected_list, [](const EntryMetadata& metadata) {
                      return AllOf(
                          Field(&EntryMetadata::type, metadata.type),
                          Field(&EntryMetadata::type_name, metadata.type_name),
                          Field(&EntryMetadata::value, metadata.value));
                    })));
          });
  EXPECT_THAT(search_results.entries, ElementsAreArray(entry_matchers));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AtMemoryQueryServiceReorderMetadataTest,
    Values(
        // Reorders secondary metadata attributes by uniqueness (more unique
        // values first).
        ReorderMetadataTestCase{
            .input_metadata_per_result =
                {{{MemoryDataType::kAddressState, u"State", u"IL"},
                  {MemoryDataType::kAddressCountry, u"Country",
                   u"United States"},
                  {MemoryDataType::kAddressCity, u"City", u"Springfield"}},
                 {{MemoryDataType::kAddressCountry, u"Country",
                   u"United States"},
                  {MemoryDataType::kAddressCity, u"City", u"Boston"},
                  {MemoryDataType::kAddressState, u"State", u"MA"}},
                 {{MemoryDataType::kAddressState, u"State", u"IL"},
                  {MemoryDataType::kAddressCity, u"City", u"Chicago"},
                  {MemoryDataType::kAddressCountry, u"Country",
                   u"United States"}}},
            .expected_metadata_per_result =
                {{{MemoryDataType::kAddressCity, u"City", u"Springfield"},
                  {MemoryDataType::kAddressState, u"State", u"IL"},
                  {MemoryDataType::kAddressCountry, u"Country",
                   u"United States"}},
                 {{MemoryDataType::kAddressCity, u"City", u"Boston"},
                  {MemoryDataType::kAddressState, u"State", u"MA"},
                  {MemoryDataType::kAddressCountry, u"Country",
                   u"United States"}},
                 {{MemoryDataType::kAddressCity, u"City", u"Chicago"},
                  {MemoryDataType::kAddressState, u"State", u"IL"},
                  {MemoryDataType::kAddressCountry, u"Country",
                   u"United States"}}}},
        // Preserves original order on tie when frequency scores are equal.
        ReorderMetadataTestCase{
            .input_metadata_per_result =
                {{{MemoryDataType::kAddressCity, u"City", u"CommonVal"},
                  {MemoryDataType::kAddressCity, u"City", u"AlsoCommonVal"},
                  {MemoryDataType::kAddressCity, u"City", u"UniqueVal"}},
                 {{MemoryDataType::kAddressCity, u"City", u"CommonVal"},
                  {MemoryDataType::kAddressCity, u"City", u"AlsoCommonVal"}}},
            .expected_metadata_per_result =
                {{{MemoryDataType::kAddressCity, u"City", u"UniqueVal"},
                  {MemoryDataType::kAddressCity, u"City", u"CommonVal"},
                  {MemoryDataType::kAddressCity, u"City", u"AlsoCommonVal"}},
                 {{MemoryDataType::kAddressCity, u"City", u"CommonVal"},
                  {MemoryDataType::kAddressCity, u"City", u"AlsoCommonVal"}}}},
        // Counts values of different types independently (e.g., SFO in
        // departure airport vs. SFO in arrival airport).
        ReorderMetadataTestCase{
            .input_metadata_per_result =
                {{{MemoryDataType::kFlightReservationDepartureAirport,
                   u"Departure Airport", u"LAX"},
                  {MemoryDataType::kFlightReservationDepartureAirport,
                   u"Departure Airport", u"SFO"}},
                 {{MemoryDataType::kFlightReservationDepartureAirport,
                   u"Departure Airport", u"LAX"},
                  {MemoryDataType::kFlightReservationDepartureAirport,
                   u"Departure Airport", u"SFO"}},
                 {{MemoryDataType::kFlightReservationDepartureAirport,
                   u"Departure Airport", u"LAX"},
                  {MemoryDataType::kFlightReservationArrivalAirport,
                   u"Arrival Airport", u"SFO"}}},
            .expected_metadata_per_result = {
                {{MemoryDataType::kFlightReservationDepartureAirport,
                  u"Departure Airport", u"SFO"},
                 {MemoryDataType::kFlightReservationDepartureAirport,
                  u"Departure Airport", u"LAX"}},
                {{MemoryDataType::kFlightReservationDepartureAirport,
                  u"Departure Airport", u"SFO"},
                 {MemoryDataType::kFlightReservationDepartureAirport,
                  u"Departure Airport", u"LAX"}},
                {{MemoryDataType::kFlightReservationArrivalAirport,
                  u"Arrival Airport", u"SFO"},
                 {MemoryDataType::kFlightReservationDepartureAirport,
                  u"Departure Airport", u"LAX"}}}}));

}  // namespace

}  // namespace autofill
