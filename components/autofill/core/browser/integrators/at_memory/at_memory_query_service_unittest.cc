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
#include "base/system/sys_info.h"
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
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/mock_personal_context_eligibility_service.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using ::base::test::ErrorIs;
using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::personal_context::proto::Any;
using ::personal_context::proto::AtMemoryQueryResponse;
using ::personal_context::proto::AtMemorySearchResult;
using ::personal_context::proto::Attribute;
using ::personal_context::proto::AutofillFetchPlan;
using ::personal_context::proto::AutofillFetchSpecification;
using ::personal_context::proto::Date;
using ::personal_context::proto::Entity;
using ::personal_context::proto::FetchPiiEntitiesResponse;
using ::personal_context::proto::TypedValue;
using Filter = ::personal_context::proto::AutofillFetchSpecification::Filter;
using StringFilter =
    ::personal_context::proto::AutofillFetchSpecification::StringFilter;
using TypedValueFilter =
    ::personal_context::proto::AutofillFetchSpecification::TypedValueFilter;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ContainsRegex;
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::Values;
using ::testing::WithParamInterface;

// Matches a `MemorySearchResult` whose `value` matches `value_matcher`.
auto SearchResultWithValue(auto value_matcher) {
  return Field(&MemorySearchResult::value, value_matcher);
}

// Matches a successful `MemorySearchResults` whose `entries` match `matchers`.
template <typename... Matchers>
auto SuccessfulSearchResults(Matchers&&... matchers) {
  return AllOf(Field(&MemorySearchResults::status,
                     MemorySearchStatus::kFinalResponseSuccess),
               Field(&MemorySearchResults::entries,
                     ElementsAre(std::forward<Matchers>(matchers)...)));
}

TypedValue CreateDateTypedValue(int year, int month, int day) {
  TypedValue typed_value;
  typed_value.mutable_date()->set_year(year);
  typed_value.mutable_date()->set_month(month);
  typed_value.mutable_date()->set_day(day);
  return typed_value;
}

TypedValue CreateDateTimeTypedValue(int year,
                                    int month,
                                    int day,
                                    int hours = 0,
                                    int minutes = 0,
                                    int seconds = 0) {
  TypedValue typed_value;
  auto* dt = typed_value.mutable_date_time();
  dt->set_year(year);
  dt->set_month(month);
  dt->set_day(day);
  dt->set_hours(hours);
  dt->set_minutes(minutes);
  dt->set_seconds(seconds);
  return typed_value;
}

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
  AtMemoryQueryServiceTest() {
    InitAutofillClient();
    personal_context::prefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterIntegerPref(
        subscription_eligibility::prefs::kAiSubscriptionTier, 0);
    ON_CALL(mock_eligibility_service_, GetNonEligibilityReason)
        .WillByDefault(testing::Return(
            personal_context::PersonalContextNonEligibilityReason::kEligible));
  }

 protected:
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void StubFetchContextResponse(AtMemoryQueryResponse response) {
    Any serialized_response;
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

  AtMemoryQueryResponse CreateQueryResponse() {
    AtMemoryQueryResponse response;
    response.set_query_classification(
        AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY);
    return response;
  }

  AtMemoryQueryResponse CreateQueryResponseWithSchemafulKey(
      personal_context::proto::MemoryDataType type,
      const std::string& value,
      double relevance_score = 1.0) {
    AtMemoryQueryResponse response = CreateQueryResponse();
    AtMemorySearchResult* result_proto = response.add_results();
    result_proto->set_relevance_score(relevance_score);
    Attribute* primary = result_proto->mutable_primary_attribute();
    primary->set_schemaful_key(type);
    primary->set_value(value);
    return response;
  }

  AtMemoryQueryResponse CreateQueryResponseWithSchemalessKey(
      const std::string& key,
      const std::string& value,
      double relevance_score = 1.0) {
    AtMemoryQueryResponse response = CreateQueryResponse();
    AtMemorySearchResult* result_proto = response.add_results();
    result_proto->set_relevance_score(relevance_score);
    Attribute* primary = result_proto->mutable_primary_attribute();
    primary->set_schemaless_key(key);
    primary->set_value(value);
    return response;
  }

  std::unique_ptr<AtMemoryQueryService> CreateQueryService(
      std::unique_ptr<AutofillDataProvider> data_provider =
          std::make_unique<FakeMemoryDataProvider>(),
      LogRouter* log_router = nullptr) {
    return std::make_unique<AtMemoryQueryService>(
        std::move(data_provider), &mock_service_, "en-US",
        &mock_eligibility_service_, &subscription_eligibility_service_,
        &pref_service_, log_router);
  }

  std::unique_ptr<AtMemoryQueryService> CreateQueryProviderWithResults(
      std::vector<MemorySearchResult> results) {
    auto data_provider = std::make_unique<FakeMemoryDataProvider>();
    data_provider->SetResults(std::move(results));
    return CreateQueryService(std::move(data_provider));
  }

  MemorySearchResults RunDeduplicationQueryWithLocalResults(
      const std::vector<MemorySearchResult>& local_results) {
    AtMemoryQueryResponse response = CreateQueryResponse();
    AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
    for (const MemorySearchResult& local_result : local_results) {
      personal_context::proto::MemoryDataType proto_type =
          personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL;
      if (local_result.type == MemoryDataType::kPassportName) {
        proto_type = personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NAME;
      } else if (local_result.type == MemoryDataType::kPassportNumber) {
        proto_type = personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER;
      } else if (local_result.type == MemoryDataType::kDriversLicenseNumber) {
        proto_type =
            personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_NUMBER;
      } else if (local_result.type ==
                 MemoryDataType::kFlightReservationDepartureDate) {
        proto_type = personal_context::proto::
            MEMORY_DATA_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE;
      } else if (local_result.type ==
                 MemoryDataType::kFlightReservationFlightNumber) {
        proto_type = personal_context::proto::
            MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER;
      } else if (local_result.type == MemoryDataType::kPhone) {
        proto_type = personal_context::proto::MEMORY_DATA_TYPE_PHONE;
      } else if (local_result.type == MemoryDataType::kAddressFull) {
        proto_type = personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL;
      }
      plan->add_fetch_specifications()->set_data_type(proto_type);
    }
    StubFetchContextResponse(std::move(response));

    std::unique_ptr<AtMemoryQueryService> service =
        CreateQueryProviderWithResults(local_results);

    base::test::TestFuture<MemorySearchResults> future;
    service->Query(u"what is my name", GURL("https://example.com"),
                   u"Page Title", future.GetRepeatingCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  personal_context::MockPersonalContextService& mock_service() {
    return mock_service_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  TestingPrefServiceSimple& pref_service() { return pref_service_; }
  subscription_eligibility::SubscriptionEligibilityService&
  subscription_eligibility_service() {
    return subscription_eligibility_service_;
  }
  personal_context::MockPersonalContextEligibilityService&
  mock_eligibility_service() {
    return mock_eligibility_service_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<personal_context::MockPersonalContextService> mock_service_;
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
  subscription_eligibility::SubscriptionEligibilityService
      subscription_eligibility_service_{&pref_service_};
  NiceMock<personal_context::MockPersonalContextEligibilityService>
      mock_eligibility_service_;
};

// Tests that the query service returns an internal failure status after
// shutdown.
TEST_F(AtMemoryQueryServiceTest, Query_AfterShutdown) {
  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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
  AtMemoryQueryResponse response = CreateQueryResponse();
  AtMemorySearchResult* result_proto = response.add_results();
  Attribute* primary = result_proto->mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
  primary->set_value("Alice");
  result_proto->set_relevance_score(0.9);
  StubFetchContextResponse(std::move(response));

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(/*data_provider=*/nullptr);

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

// Tests that local Autofill results precede remote results in the final output.
TEST_F(AtMemoryQueryServiceTest, Query_LocalResultsPrecedeRemoteResults) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  plan->add_fetch_specifications()->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  AtMemorySearchResult* remote_result = response.add_results();
  Attribute* primary = remote_result->mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
  primary->set_value("Remote Name");
  remote_result->set_relevance_score(0.9);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

  MemorySearchResult local_name(MemoryDataType::kNameFull, u"Name",
                                u"Local Name");
  fake_data_provider->SetResults({local_name});

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(
      result.entries,
      ElementsAre(AllOf(Field(&MemorySearchResult::value, u"Local Name"),
                        Field(&MemorySearchResult::remote_response_index,
                              std::nullopt)),
                  AllOf(Field(&MemorySearchResult::value, u"Remote Name"),
                        Field(&MemorySearchResult::remote_response_index, 0))));
}

// Tests that the query service returns the appropriate error status when the
// personal context resolver fails.
TEST_F(AtMemoryQueryServiceTest, Query_PersonalContextResolverError) {
  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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
  AtMemoryQueryResponse response;
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  plan->add_fetch_specifications()->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
  response.set_query_classification(
      AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY);

  Any serialized_response1;
  serialized_response1.set_value(response.SerializeAsString());
  personal_context::FetchContextResult result1(std::move(serialized_response1));
  auto shared_result1 = std::make_shared<personal_context::FetchContextResult>(
      std::move(result1));

  Any serialized_response2;
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
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

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
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  plan->add_fetch_specifications()->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

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
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  plan->add_fetch_specifications()->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

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
  EXPECT_THAT(
      result.entries,
      ElementsAre(
          AllOf(Field(&MemorySearchResult::value, u"John Doe"),
                Field(&MemorySearchResult::confidence_score, DoubleEq(0.9)),
                Field(&MemorySearchResult::is_local, true),
                Field(&MemorySearchResult::sources,
                      ElementsAre(Field(&MemoryEntrySource::type,
                                        MemoryEntrySourceType::kAutofill))))));
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
  MemorySearchResult result4(MemoryDataType::kPhone, u"Phone", u"John Doe");
  result4.metadata_list.push_back(sd_meta);

  const MemorySearchResults& result = RunDeduplicationQueryWithLocalResults(
      {result1, result2, result3, result4});
  EXPECT_THAT(
      result.entries,
      ElementsAre(
          AllOf(Field(&MemorySearchResult::value, u"John Doe"),
                Field(&MemorySearchResult::type, MemoryDataType::kNameFull),
                Field(&MemorySearchResult::metadata_list,
                      ElementsAre(Field(&EntryMetadata::value, u"San Diego")))),
          AllOf(Field(&MemorySearchResult::value, u"John Doe"),
                Field(&MemorySearchResult::type, MemoryDataType::kNameFull),
                Field(&MemorySearchResult::metadata_list,
                      ElementsAre(Field(&EntryMetadata::value, u"New York")))),
          AllOf(Field(&MemorySearchResult::value, u"Jane Doe"),
                Field(&MemorySearchResult::type, MemoryDataType::kNameFull),
                Field(&MemorySearchResult::metadata_list,
                      ElementsAre(Field(&EntryMetadata::value, u"San Diego")))),
          AllOf(
              Field(&MemorySearchResult::value, u"John Doe"),
              Field(&MemorySearchResult::type, MemoryDataType::kPhone),
              Field(&MemorySearchResult::metadata_list,
                    ElementsAre(Field(&EntryMetadata::value, u"San Diego"))))));
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

// Tests that results with matching metadata `TypedValue`s are deduplicated,
// even if their string representations differ in formatting.
TEST_F(AtMemoryQueryServiceTest, Query_DeduplicatesResults_TypedValueMatching) {
  TypedValue datetime1;
  datetime1.mutable_date_time()->set_year(2024);
  datetime1.mutable_date_time()->set_month(6);
  datetime1.mutable_date_time()->set_day(7);
  datetime1.mutable_date_time()->set_hours(15);
  datetime1.mutable_date_time()->set_minutes(30);

  TypedValue datetime2 = datetime1;

  MemorySearchResult result1(MemoryDataType::kFlightReservationFlightNumber,
                             u"Flight Number", u"FL123");
  result1.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationDepartureDate, u"Departure Date",
      u"2024-06-07 15:30", datetime1);

  MemorySearchResult result2(MemoryDataType::kFlightReservationFlightNumber,
                             u"Flight Number", u"FL123");
  result2.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationDepartureDate, u"Departure Date",
      u"2024-06-07 3:30 PM", datetime2);

  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 1u);
}

// Tests that results with mismatching metadata `TypedValue`s are not
// deduplicated.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_TypedValueMismatch_NotDeduplicated) {
  TypedValue datetime1;
  datetime1.mutable_date_time()->set_year(2024);
  datetime1.mutable_date_time()->set_month(6);
  datetime1.mutable_date_time()->set_day(7);
  datetime1.mutable_date_time()->set_hours(15);
  datetime1.mutable_date_time()->set_minutes(30);

  TypedValue datetime2;
  datetime2.mutable_date_time()->set_year(2024);
  datetime2.mutable_date_time()->set_month(6);
  datetime2.mutable_date_time()->set_day(8);
  datetime2.mutable_date_time()->set_hours(15);
  datetime2.mutable_date_time()->set_minutes(30);

  MemorySearchResult result1(MemoryDataType::kFlightReservationFlightNumber,
                             u"Flight Number", u"FL123");
  result1.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationDepartureDate, u"Departure Date",
      u"2024-06-07 15:30", datetime1);

  MemorySearchResult result2(MemoryDataType::kFlightReservationFlightNumber,
                             u"Flight Number", u"FL123");
  result2.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationDepartureDate, u"Departure Date",
      u"2024-06-08 3:30 PM", datetime2);

  const MemorySearchResults& result =
      RunDeduplicationQueryWithLocalResults({result1, result2});
  EXPECT_EQ(result.entries.size(), 2u);
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

  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  plan->add_fetch_specifications()->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"Jane Doe");
  fake_data_provider->SetResults({result1, result2});

  TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", GURL("https://example.com"), u"Page Title",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.ProviderResultCount."
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
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

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
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

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
  AtMemoryQueryResponse response;
  response.set_query_classification(
      AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY);

  // 1. Non-SPII: Full Name
  AtMemorySearchResult* result1 = response.add_results();
  result1->mutable_primary_attribute()->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
  result1->mutable_primary_attribute()->set_value("John Doe");

  // 2. SPII: Credit Card Number
  AtMemorySearchResult* result2 = response.add_results();
  result2->mutable_primary_attribute()->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NUMBER);
  result2->mutable_primary_attribute()->set_value("1111222233334444");

  StubFetchContextResponse(std::move(response));

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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
  AtMemoryQueryResponse response = CreateQueryResponseWithSchemafulKey(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL, "Remote Name");
  response.mutable_autofill_fetch_plan()
      ->add_fetch_specifications()
      ->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  MemorySearchResult local_b(MemoryDataType::kNameFull, u"Name",
                             u"Local Name B", /*confidence_score=*/0.5);
  MemorySearchResult local_a(MemoryDataType::kNameFull, u"Name",
                             u"Local Name A", /*confidence_score=*/0.9);
  fake_data_provider->SetResults({local_b, local_a});

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

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
  AtMemoryQueryResponse response = CreateQueryResponseWithSchemafulKey(
      personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER,
      "Remote 1Z12345");
  response.mutable_autofill_fetch_plan()
      ->add_fetch_specifications()
      ->set_data_type(
          personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  MemorySearchResult local_shipment(MemoryDataType::kShipmentTrackingNumber,
                                    u"Tracking", u"Local 1Z67890",
                                    /*confidence_score=*/0.8);
  fake_data_provider->SetResults({local_shipment});

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

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
  AtMemoryQueryResponse response = CreateQueryResponseWithSchemafulKey(
      personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER,
      "Remote 1Z12345");
  response.mutable_autofill_fetch_plan()
      ->add_fetch_specifications()
      ->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);

  StubFetchContextResponse(std::move(response));

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  MemorySearchResult local_address(MemoryDataType::kAddressFull, u"Address",
                                   u"123 Main St", /*confidence_score=*/0.7);
  fake_data_provider->SetResults({local_address});

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(data_provider));

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
  AtMemoryQueryResponse::QueryClassification classification;
  MemorySearchStatus expected_status;
};

class AtMemoryQueryServiceClassificationTest
    : public AtMemoryQueryServiceTest,
      public WithParamInterface<QueryClassificationTestCase> {};

// Verifies that each query classification is correctly mapped to a search
// status.
TEST_P(AtMemoryQueryServiceClassificationTest, MapQueryClassificationToStatus) {
  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

  AtMemoryQueryResponse response;
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
        {AtMemoryQueryResponse::QUERY_CLASSIFICATION_UNSPECIFIED,
         MemorySearchStatus::kInternalFailure},
        {AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY,
         MemorySearchStatus::kFinalResponseSuccess},
        {AtMemoryQueryResponse::QUERY_CLASSIFICATION_UNSUPPORTED,
         MemorySearchStatus::kUnsupportedQuery},
        {AtMemoryQueryResponse::QUERY_CLASSIFICATION_SENSITIVE,
         MemorySearchStatus::kUnsupportedQuery},
        {AtMemoryQueryResponse::QUERY_CLASSIFICATION_RECITATION,
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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();
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

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(/*data_provider=*/nullptr);

  service->Query(u"Alice", GURL("https://example.com/"), u"Example Title",
                 base::DoNothing());
}

// Tests that when there is no device authenticator,
// `AuthenticateAndFetchPiiEntity(..)` returns a `kReauthFailed` error and does
// not fetch from `PersonalContextService`.
TEST_F(AtMemoryQueryServiceTest,
       AuthenticateAndFetchPiiEntity_NoAuthenticator) {
  autofill_client().set_device_authenticator(nullptr);
  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

  FetchPiiEntitiesResponse response;
  Entity* entity = response.add_entities();
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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

  FetchPiiEntitiesResponse response;
  Entity* entity = response.add_entities();
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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService();

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
      public WithParamInterface<ReorderMetadataTestCase> {};

// Tests that secondary metadata attributes in query search results are
// reordered by uniqueness across all suggestions (more unique values of the
// same type first), preserving original relative provider order when frequency
// scores tie.
TEST_P(AtMemoryQueryServiceReorderMetadataTest, ReordersSecondaryMetadata) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  response.mutable_autofill_fetch_plan()
      ->add_fetch_specifications()
      ->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
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
      CreateQueryService(std::move(data_provider));

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
            .expected_metadata_per_result =
                {{{MemoryDataType::kFlightReservationDepartureAirport,
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
                   u"Departure Airport", u"LAX"}}}},
        // Reorders schemaless metadata attributes by uniqueness (more unique
        // values first).
        ReorderMetadataTestCase{
            .input_metadata_per_result =
                {{{MemoryDataType::kUnknown, u"Gate", u"A1"},
                  {MemoryDataType::kUnknown, u"Terminal", u"T1"},
                  {MemoryDataType::kUnknown, u"Seat", u"12A"}},
                 {{MemoryDataType::kUnknown, u"Gate", u"A1"},
                  {MemoryDataType::kUnknown, u"Terminal", u"T1"},
                  {MemoryDataType::kUnknown, u"Seat", u"14B"}},
                 {{MemoryDataType::kUnknown, u"Gate", u"A1"},
                  {MemoryDataType::kUnknown, u"Terminal", u"T2"},
                  {MemoryDataType::kUnknown, u"Seat", u"16C"}}},
            .expected_metadata_per_result =
                {{{MemoryDataType::kUnknown, u"Seat", u"12A"},
                  {MemoryDataType::kUnknown, u"Terminal", u"T1"},
                  {MemoryDataType::kUnknown, u"Gate", u"A1"}},
                 {{MemoryDataType::kUnknown, u"Seat", u"14B"},
                  {MemoryDataType::kUnknown, u"Terminal", u"T1"},
                  {MemoryDataType::kUnknown, u"Gate", u"A1"}},
                 {{MemoryDataType::kUnknown, u"Terminal", u"T2"},
                  {MemoryDataType::kUnknown, u"Seat", u"16C"},
                  {MemoryDataType::kUnknown, u"Gate", u"A1"}}}},
        // Counts values of different schemaless type names independently (e.g.,
        // "A1" in Gate vs. "A1" in Seat).
        ReorderMetadataTestCase{
            .input_metadata_per_result =
                {{{MemoryDataType::kUnknown, u"Gate", u"A1"},
                  {MemoryDataType::kUnknown, u"Gate", u"B2"}},
                 {{MemoryDataType::kUnknown, u"Gate", u"A1"},
                  {MemoryDataType::kUnknown, u"Gate", u"B2"}},
                 {{MemoryDataType::kUnknown, u"Gate", u"A1"},
                  {MemoryDataType::kUnknown, u"Seat", u"B2"}}},
            .expected_metadata_per_result =
                {{{MemoryDataType::kUnknown, u"Gate", u"B2"},
                  {MemoryDataType::kUnknown, u"Gate", u"A1"}},
                 {{MemoryDataType::kUnknown, u"Gate", u"B2"},
                  {MemoryDataType::kUnknown, u"Gate", u"A1"}},
                 {{MemoryDataType::kUnknown, u"Seat", u"B2"},
                  {MemoryDataType::kUnknown, u"Gate", u"A1"}}}},
        // Mixed schemaful and schemaless metadata are properly reordered
        // together by uniqueness.
        ReorderMetadataTestCase{
            .input_metadata_per_result =
                {{{MemoryDataType::kAddressCountry, u"Country",
                   u"United States"},
                  {MemoryDataType::kUnknown, u"Neighborhood", u"North End"},
                  {MemoryDataType::kAddressState, u"State", u"MA"}},
                 {{MemoryDataType::kAddressCountry, u"Country",
                   u"United States"},
                  {MemoryDataType::kUnknown, u"Neighborhood", u"Back Bay"},
                  {MemoryDataType::kAddressState, u"State", u"MA"}},
                 {{MemoryDataType::kAddressCountry, u"Country",
                   u"United States"},
                  {MemoryDataType::kUnknown, u"Neighborhood", u"North End"},
                  {MemoryDataType::kAddressState, u"State", u"IL"}}},
            .expected_metadata_per_result = {
                {{MemoryDataType::kUnknown, u"Neighborhood", u"North End"},
                 {MemoryDataType::kAddressState, u"State", u"MA"},
                 {MemoryDataType::kAddressCountry, u"Country",
                  u"United States"}},
                {{MemoryDataType::kUnknown, u"Neighborhood", u"Back Bay"},
                 {MemoryDataType::kAddressState, u"State", u"MA"},
                 {MemoryDataType::kAddressCountry, u"Country",
                  u"United States"}},
                {{MemoryDataType::kAddressState, u"State", u"IL"},
                 {MemoryDataType::kUnknown, u"Neighborhood", u"North End"},
                 {MemoryDataType::kAddressCountry, u"Country",
                  u"United States"}}}}));

// Tests that local results are filtered using string filters in fetch
// specifications.
TEST_F(AtMemoryQueryServiceTest,
       Query_FiltersLocalDataUsingFetchSpecifications_StringFilter) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  auto* filter = spec->add_filters();
  filter->mutable_string_filter()->set_value("home");

  StubFetchContextResponse(std::move(response));

  MemorySearchResult match_entry(MemoryDataType::kAddressFull, u"Home Address",
                                 u"123 Main St Home");
  MemorySearchResult reject_entry(MemoryDataType::kAddressFull, u"Work Address",
                                  u"456 Market St Work");
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({match_entry, reject_entry});

  TestFuture<MemorySearchResults> future;
  service->Query(u"address", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(), SuccessfulSearchResults(
                                SearchResultWithValue(u"123 Main St Home")));
}

// Tests that exact string filter mode requires the full string to match
// case-insensitively.
TEST_F(AtMemoryQueryServiceTest,
       Query_FiltersLocalDataUsingFetchSpecifications_StringFilterExactMode) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  auto* filter = spec->add_filters();
  filter->mutable_string_filter()->set_value("123 Main St");
  filter->mutable_string_filter()->set_mode(
      AutofillFetchSpecification::StringFilter::STRING_FILTER_MODE_EXACT);

  StubFetchContextResponse(std::move(response));

  MemorySearchResult match_entry(MemoryDataType::kAddressFull, u"Address",
                                 u"123 MAIN ST");
  MemorySearchResult partial_entry(MemoryDataType::kAddressFull, u"Address",
                                   u"123 Main St Apt 4");
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({match_entry, partial_entry});

  TestFuture<MemorySearchResults> future;
  service->Query(u"address", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              SuccessfulSearchResults(SearchResultWithValue(u"123 MAIN ST")));
}

// Tests that string filters apply Unicode and case normalization when matching.
TEST_F(AtMemoryQueryServiceTest,
       Query_FiltersLocalDataUsingFetchSpecifications_StringFilterNormalized) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NAME);
  auto* filter = spec->add_filters();
  filter->mutable_string_filter()->set_value("Timothé");
  filter->mutable_string_filter()->set_mode(
      AutofillFetchSpecification::StringFilter::STRING_FILTER_MODE_EXACT);

  StubFetchContextResponse(std::move(response));

  MemorySearchResult match_entry(MemoryDataType::kPassportName,
                                 u"Passport Name", u"timothe");
  MemorySearchResult reject_entry(MemoryDataType::kPassportName,
                                  u"Passport Name", u"Someone Else");
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({match_entry, reject_entry});

  TestFuture<MemorySearchResults> future;
  service->Query(u"passport", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              SuccessfulSearchResults(SearchResultWithValue(u"timothe")));
}

// Tests that fuzzy string filter mode matches entries with minor typos or
// missing words.
TEST_F(AtMemoryQueryServiceTest,
       Query_FiltersLocalDataUsingFetchSpecifications_StringFilterFuzzyMode) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  auto* filter = spec->add_filters();
  filter->mutable_string_filter()->set_value("123 Main Stret");
  filter->mutable_string_filter()->set_mode(
      AutofillFetchSpecification::StringFilter::STRING_FILTER_MODE_FUZZY);

  StubFetchContextResponse(std::move(response));

  MemorySearchResult match_entry(MemoryDataType::kAddressFull, u"Address",
                                 u"123 Main Street");
  MemorySearchResult reject_entry(MemoryDataType::kAddressFull, u"Address",
                                  u"456 Market Road");
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({match_entry, reject_entry});

  TestFuture<MemorySearchResults> future;
  service->Query(u"address", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(), SuccessfulSearchResults(
                                SearchResultWithValue(u"123 Main Street")));
}

// Tests that local results are filtered using typed value filters in fetch
// specifications.
TEST_F(AtMemoryQueryServiceTest,
       Query_FiltersLocalDataUsingFetchSpecifications_TypedFilter) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  auto* filter = spec->add_filters();
  filter->mutable_typed_value_filter()->mutable_typed_value()->set_country_code(
      "US");

  StubFetchContextResponse(std::move(response));

  MemorySearchResult match_entry(MemoryDataType::kAddressFull, u"US Address",
                                 u"123 Main St");
  EntryMetadata match_meta(MemoryDataType::kAddressCountry, u"Country",
                           u"United States");
  match_meta.typed_value.emplace().set_country_code("US");
  match_entry.metadata_list.push_back(std::move(match_meta));

  MemorySearchResult reject_entry(MemoryDataType::kAddressFull, u"CA Address",
                                  u"456 Queen St");
  EntryMetadata reject_meta(MemoryDataType::kAddressCountry, u"Country",
                            u"Canada");
  reject_meta.typed_value.emplace().set_country_code("CA");
  reject_entry.metadata_list.push_back(std::move(reject_meta));

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({match_entry, reject_entry});

  TestFuture<MemorySearchResults> future;
  service->Query(u"address", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              SuccessfulSearchResults(SearchResultWithValue(u"123 Main St")));
}

// Tests that date typed filters match entries when unset date components act as
// wildcards.
TEST_F(
    AtMemoryQueryServiceTest,
    Query_FiltersLocalDataUsingFetchSpecifications_TypedFilterDateWildcards) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);
  auto* filter = spec->add_filters();
  filter->mutable_typed_value_filter()
      ->mutable_typed_value()
      ->mutable_date()
      ->set_year(2024);
  filter->mutable_typed_value_filter()
      ->mutable_typed_value()
      ->mutable_date()
      ->set_month(5);

  StubFetchContextResponse(std::move(response));

  MemorySearchResult match_passport(MemoryDataType::kPassportNumber,
                                    u"Passport Number", u"PASSPORT_MATCH");
  EntryMetadata match_meta(MemoryDataType::kPassportIssueDate, u"Issue Date",
                           u"2024-05-15");
  match_meta.typed_value = CreateDateTypedValue(2024, 5, 15);
  match_passport.metadata_list.push_back(std::move(match_meta));

  MemorySearchResult reject_passport(MemoryDataType::kPassportNumber,
                                     u"Passport Number", u"PASSPORT_REJECT");
  EntryMetadata reject_meta(MemoryDataType::kPassportIssueDate, u"Issue Date",
                            u"2024-06-15");
  reject_meta.typed_value = CreateDateTypedValue(2024, 6, 15);
  reject_passport.metadata_list.push_back(std::move(reject_meta));

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({match_passport, reject_passport});

  TestFuture<MemorySearchResults> future;
  service->Query(u"passport", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(), SuccessfulSearchResults(
                                SearchResultWithValue(u"PASSPORT_MATCH")));
}

// Tests that typed filters support relational operators (LESS_THAN,
// GREATER_THAN, etc.)
TEST_F(AtMemoryQueryServiceTest,
       Query_FiltersLocalDataUsingFetchSpecifications_FilterOperators) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(personal_context::proto::
                          MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER);
  auto* filter = spec->add_filters();
  filter->mutable_typed_value_filter()
      ->mutable_typed_value()
      ->mutable_date()
      ->set_year(2026);
  filter->mutable_typed_value_filter()
      ->mutable_typed_value()
      ->mutable_date()
      ->set_month(10);
  filter->mutable_typed_value_filter()->set_filter_operator(
      AutofillFetchSpecification::TypedValueFilter::
          FILTER_OPERATOR_LESS_THAN_OR_EQUAL);

  StubFetchContextResponse(std::move(response));

  MemorySearchResult oct_flight(MemoryDataType::kFlightReservationFlightNumber,
                                u"Flight Number", u"FLIGHT_OCT");
  EntryMetadata oct_meta(MemoryDataType::kFlightReservationDepartureDate,
                         u"Departure Date", u"2026-10-15");
  oct_meta.typed_value = CreateDateTypedValue(2026, 10, 15);
  oct_flight.metadata_list.push_back(std::move(oct_meta));

  MemorySearchResult nov_flight(MemoryDataType::kFlightReservationFlightNumber,
                                u"Flight Number", u"FLIGHT_NOV");
  EntryMetadata nov_meta(MemoryDataType::kFlightReservationDepartureDate,
                         u"Departure Date", u"2026-11-01");
  nov_meta.typed_value = CreateDateTypedValue(2026, 11, 1);
  nov_flight.metadata_list.push_back(std::move(nov_meta));

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({oct_flight, nov_flight});

  TestFuture<MemorySearchResults> future;
  service->Query(u"flight", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              SuccessfulSearchResults(SearchResultWithValue(u"FLIGHT_OCT")));
}

// Tests cross-type matching between Date and DateTime typed values.
TEST_F(
    AtMemoryQueryServiceTest,
    Query_FiltersLocalDataUsingFetchSpecifications_DateAndDateTimeCrossMatching) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(personal_context::proto::
                          MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER);
  auto* filter = spec->add_filters();
  filter->mutable_typed_value_filter()
      ->mutable_typed_value()
      ->mutable_date()
      ->set_year(2026);
  filter->mutable_typed_value_filter()
      ->mutable_typed_value()
      ->mutable_date()
      ->set_month(10);
  filter->mutable_typed_value_filter()->set_filter_operator(
      AutofillFetchSpecification::TypedValueFilter::
          FILTER_OPERATOR_LESS_THAN_OR_EQUAL);

  StubFetchContextResponse(std::move(response));

  MemorySearchResult datetime_flight(
      MemoryDataType::kFlightReservationFlightNumber, u"Flight Number",
      u"FLIGHT_DATETIME_MATCH");
  EntryMetadata dt_meta(MemoryDataType::kFlightReservationDepartureDate,
                        u"Departure Date", u"2026-10-15T14:30:00");
  dt_meta.typed_value = CreateDateTimeTypedValue(2026, 10, 15, 14, 30, 0);
  datetime_flight.metadata_list.push_back(std::move(dt_meta));

  MemorySearchResult nov_flight(MemoryDataType::kFlightReservationFlightNumber,
                                u"Flight Number", u"FLIGHT_NOV");
  EntryMetadata nov_meta(MemoryDataType::kFlightReservationDepartureDate,
                         u"Departure Date", u"2026-11-01T09:00:00");
  nov_meta.typed_value = CreateDateTimeTypedValue(2026, 11, 1, 9, 0, 0);
  nov_flight.metadata_list.push_back(std::move(nov_meta));

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({datetime_flight, nov_flight});

  TestFuture<MemorySearchResults> future;
  service->Query(u"flight", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(
      future.Get(),
      SuccessfulSearchResults(SearchResultWithValue(u"FLIGHT_DATETIME_MATCH")));
}

// Tests that unset time fields in a DateTime filter act as wildcards, whereas
// explicitly setting time fields to 0 filters strictly for midnight (00:00:00).
TEST_F(
    AtMemoryQueryServiceTest,
    Query_FiltersLocalDataUsingFetchSpecifications_DateTimeTimeWildcardVsZero) {
  MemorySearchResult midnight_flight(
      MemoryDataType::kFlightReservationFlightNumber, u"Flight Number",
      u"FLIGHT_MIDNIGHT");
  EntryMetadata midnight_meta(MemoryDataType::kFlightReservationDepartureDate,
                              u"Departure Date", u"2026-10-15T00:00:00");
  midnight_meta.typed_value = CreateDateTimeTypedValue(2026, 10, 15, 0, 0, 0);
  midnight_flight.metadata_list.push_back(std::move(midnight_meta));

  MemorySearchResult afternoon_flight(
      MemoryDataType::kFlightReservationFlightNumber, u"Flight Number",
      u"FLIGHT_AFTERNOON");
  EntryMetadata afternoon_meta(MemoryDataType::kFlightReservationDepartureDate,
                               u"Departure Date", u"2026-10-15T14:30:00");
  afternoon_meta.typed_value =
      CreateDateTimeTypedValue(2026, 10, 15, 14, 30, 0);
  afternoon_flight.metadata_list.push_back(std::move(afternoon_meta));

  // Case 1: Filter with UNSET time fields (acting as wildcards).
  // Both midnight_flight and afternoon_flight should match.
  {
    AtMemoryQueryResponse response = CreateQueryResponse();
    AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
    auto* spec = plan->add_fetch_specifications();
    spec->set_data_type(personal_context::proto::
                            MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER);
    auto* filter = spec->add_filters();
    auto* dt = filter->mutable_typed_value_filter()
                   ->mutable_typed_value()
                   ->mutable_date_time();
    dt->set_year(2026);
    dt->set_month(10);
    dt->set_day(15);
    filter->mutable_typed_value_filter()->set_filter_operator(
        AutofillFetchSpecification::TypedValueFilter::FILTER_OPERATOR_EQUAL);

    StubFetchContextResponse(std::move(response));

    std::unique_ptr<AtMemoryQueryService> service =
        CreateQueryProviderWithResults({midnight_flight, afternoon_flight});

    TestFuture<MemorySearchResults> future;
    service->Query(u"flight", GURL("https://example.com"), u"Title",
                   future.GetRepeatingCallback());

    EXPECT_THAT(future.Get(), SuccessfulSearchResults(
                                  SearchResultWithValue(u"FLIGHT_MIDNIGHT"),
                                  SearchResultWithValue(u"FLIGHT_AFTERNOON")));
  }

  // Case 2: Filter with EXPLICITLY SET time fields set to 0 (hours = 0, minutes
  // = 0). Only midnight_flight should match; afternoon_flight must be rejected.
  {
    AtMemoryQueryResponse response = CreateQueryResponse();
    AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
    auto* spec = plan->add_fetch_specifications();
    spec->set_data_type(personal_context::proto::
                            MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER);
    auto* filter = spec->add_filters();
    auto* dt = filter->mutable_typed_value_filter()
                   ->mutable_typed_value()
                   ->mutable_date_time();
    dt->set_year(2026);
    dt->set_month(10);
    dt->set_day(15);
    dt->set_hours(0);
    dt->set_minutes(0);
    filter->mutable_typed_value_filter()->set_filter_operator(
        AutofillFetchSpecification::TypedValueFilter::FILTER_OPERATOR_EQUAL);

    StubFetchContextResponse(std::move(response));

    std::unique_ptr<AtMemoryQueryService> service =
        CreateQueryProviderWithResults({midnight_flight, afternoon_flight});

    TestFuture<MemorySearchResults> future;
    service->Query(u"flight", GURL("https://example.com"), u"Title",
                   future.GetRepeatingCallback());

    EXPECT_THAT(future.Get(), SuccessfulSearchResults(
                                  SearchResultWithValue(u"FLIGHT_MIDNIGHT")));
  }
}

// Tests that filters only match against fields whose data types are in the
// filter's allowed data types.
TEST_F(AtMemoryQueryServiceTest,
       Query_FiltersLocalDataUsingFetchSpecifications_DataTypesRestriction) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);
  auto* filter = spec->add_filters();
  filter->mutable_string_filter()->set_value("United");
  filter->add_data_types(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);

  StubFetchContextResponse(std::move(response));

  MemorySearchResult entry(MemoryDataType::kPassportNumber, u"Passport Number",
                           u"XYZ123");
  entry.metadata_list.emplace_back(MemoryDataType::kPassportCountry,
                                   u"Passport Country", u"United States");
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({entry});

  TestFuture<MemorySearchResults> future;
  service->Query(u"passport", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(), SuccessfulSearchResults());
}

// Tests that all filters in a fetch specification must match for an entry to be
// returned.
TEST_F(AtMemoryQueryServiceTest,
       Query_FiltersLocalDataUsingFetchSpecifications_MultipleFiltersAnd) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);
  auto* filter1 = spec->add_filters();
  filter1->mutable_string_filter()->set_value("XYZ123");
  auto* filter2 = spec->add_filters();
  filter2->mutable_typed_value_filter()
      ->mutable_typed_value()
      ->set_country_code("CA");

  StubFetchContextResponse(std::move(response));

  MemorySearchResult match_entry(MemoryDataType::kPassportNumber,
                                 u"Passport Number", u"XYZ123 Match");
  EntryMetadata match_meta(MemoryDataType::kPassportCountry,
                           u"Passport Country", u"Canada");
  match_meta.typed_value.emplace().set_country_code("CA");
  match_entry.metadata_list.push_back(std::move(match_meta));

  MemorySearchResult reject_entry(MemoryDataType::kPassportNumber,
                                  u"Passport Number", u"XYZ123 Reject");
  EntryMetadata reject_meta(MemoryDataType::kPassportCountry,
                            u"Passport Country", u"United States");
  reject_meta.typed_value.emplace().set_country_code("US");
  reject_entry.metadata_list.push_back(std::move(reject_meta));
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({match_entry, reject_entry});

  TestFuture<MemorySearchResults> future;
  service->Query(u"passport", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              SuccessfulSearchResults(SearchResultWithValue(u"XYZ123 Match")));
}

// Tests that local results matching any fetch specification in the plan are
// returned.
TEST_F(
    AtMemoryQueryServiceTest,
    Query_FiltersLocalDataUsingFetchSpecifications_MultipleSpecificationsOr) {
  AtMemoryQueryResponse response = CreateQueryResponse();
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();

  auto* spec1 = plan->add_fetch_specifications();
  spec1->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);
  spec1->add_filters()->mutable_string_filter()->set_value("XYZ");

  auto* spec2 = plan->add_fetch_specifications();
  spec2->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL);
  spec2->add_filters()->mutable_string_filter()->set_value("Main");

  StubFetchContextResponse(std::move(response));

  MemorySearchResult passport(MemoryDataType::kPassportNumber,
                              u"Passport Number", u"XYZ123");
  MemorySearchResult address(MemoryDataType::kAddressFull, u"Address",
                             u"123 Main St");
  MemorySearchResult credit_card(MemoryDataType::kCreditCardNumber,
                                 u"Credit Card", u"41111111");
  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryProviderWithResults({passport, address, credit_card});

  TestFuture<MemorySearchResults> future;
  service->Query(u"passport address", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              SuccessfulSearchResults(SearchResultWithValue(u"XYZ123"),
                                      SearchResultWithValue(u"123 Main St")));
}

// Tests that `Autofill.AtMemory.PersonalContext.NonEligibilityReason` is logged
// after a 30-second startup delay.
TEST_F(AtMemoryQueryServiceTest,
       LogsAtMemoryNonEligibilityReasonAfterStartupDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAtMemory, {{"at_memory_eligible_tiers", "1,2"}}}},
      {});

  // Before 30 seconds, startup logging should not have occurred.
  histogram_tester().ExpectTotalCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason", 0);

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US",
      &mock_eligibility_service(), &subscription_eligibility_service(),
      &pref_service(), /*log_router=*/nullptr);

  // Fast forward by 31 seconds to trigger startup logging.
  FastForwardBy(base::Seconds(31));

  histogram_tester().ExpectTotalCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason", 1);
}

// Tests that `Autofill.AtMemory.PersonalContext.NonEligibilityReason` is logged
// on subscription tier updates after the startup delay has elapsed.
TEST_F(AtMemoryQueryServiceTest, LogsAtMemoryNonEligibilityReasonOnTierChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAtMemory, {{"at_memory_eligible_tiers", "1,2"}}}},
      {});

  // Set tier to an eligible tier (1) before startup logging triggers.
  pref_service().SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US",
      &mock_eligibility_service(), &subscription_eligibility_service(),
      &pref_service(), /*log_router=*/nullptr);

  // Fast forward by 31 seconds to complete startup logging (records kEligible).
  FastForwardBy(base::Seconds(31));

  histogram_tester().ExpectBucketCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 1);

  // Then change tier to an ineligible tier (99).
  pref_service().SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 99);
  histogram_tester().ExpectBucketCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::
          kNotG1SubscriberOrAndroidPremiumDevice,
      1);
}

// Tests that `Autofill.AtMemory.PersonalContext.NonEligibilityReason` is logged
// on settings toggle updates after the startup delay has elapsed.
TEST_F(AtMemoryQueryServiceTest,
       LogsAtMemoryNonEligibilityReasonOnToggleChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAtMemory, {{"at_memory_eligible_tiers", "1,2"}}}},
      {});

  pref_service().SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US",
      &mock_eligibility_service(), &subscription_eligibility_service(),
      &pref_service(), /*log_router=*/nullptr);

  FastForwardBy(base::Seconds(31));

  histogram_tester().ExpectBucketCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 1);

  pref_service().SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);
  histogram_tester().ExpectBucketCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::
          kPersonalIntelligencePrefDisabled,
      1);

  pref_service().SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);
  histogram_tester().ExpectBucketCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 2);
}

#if BUILDFLAG(IS_ANDROID)
// Tests that `Autofill.AtMemory.PersonalContext.NonEligibilityReason` logs
// `kEligible` when the Android device is supported, even if the user's
// subscription tier is not in the eligible tiers list.
TEST_F(AtMemoryQueryServiceTest,
       LogsAtMemoryEligibilityReasonOnAndroidPremiumDevice) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAtMemory,
        {{"at_memory_eligible_tiers", "1,2"},
         {"at_memory_enabled_devices", base::SysInfo::HardwareModelName()}}}},
      {});

  // Set tier to an eligible tier (1) before startup delay.
  pref_service().SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<FakeMemoryDataProvider>(), &mock_service(), "en-US",
      &mock_eligibility_service(), &subscription_eligibility_service(),
      &pref_service(), /*log_router=*/nullptr);

  // Fast forward by 31 seconds to complete startup logging.
  FastForwardBy(base::Seconds(31));

  histogram_tester().ExpectBucketCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 1);

  // Then change tier to an ineligible tier (99). Since the Android device is
  // supported, the user remains eligible (`kEligible`), so no duplicate sample
  // is logged.
  pref_service().SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 99);
  histogram_tester().ExpectBucketCount(
      "Autofill.AtMemory.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 1);
}
#endif

class MockLogReceiver : public LogReceiver {
 public:
  MockLogReceiver() = default;
  ~MockLogReceiver() override = default;
  MOCK_METHOD(void, LogEntry, (const base::DictValue&), (override));
};

// Tests that receiving an Autofill fetch plan logs it to autofill-internals
// logs.
TEST_F(AtMemoryQueryServiceTest, LogsFetchPlanWhenReceived) {
  LogRouter log_router;
  MockLogReceiver receiver;
  log_router.RegisterReceiver(&receiver);

  std::unique_ptr<AtMemoryQueryService> service = CreateQueryService(
      std::make_unique<FakeMemoryDataProvider>(), &log_router);

  AtMemoryQueryResponse response;
  response.set_query_classification(
      AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY);
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();

  AutofillFetchSpecification* spec1 = plan->add_fetch_specifications();
  spec1->set_data_type(personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MAKE);
  AutofillFetchSpecification::Filter* filter1 = spec1->add_filters();
  filter1->add_data_types(
      personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MAKE);
  filter1->mutable_string_filter()->set_value("BMW");
  filter1->mutable_string_filter()->set_mode(
      personal_context::proto::AutofillFetchSpecification::StringFilter::
          STRING_FILTER_MODE_EXACT);

  AutofillFetchSpecification* spec2 = plan->add_fetch_specifications();
  spec2->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);
  AutofillFetchSpecification::Filter* filter2 = spec2->add_filters();
  filter2->add_data_types(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);
  filter2->mutable_string_filter()->set_value("ABC");
  filter2->mutable_string_filter()->set_mode(
      personal_context::proto::AutofillFetchSpecification::StringFilter::
          STRING_FILTER_MODE_SUBSTRING);

  StubFetchContextResponse(std::move(response));

  EXPECT_CALL(receiver, LogEntry)
      .WillOnce([](const base::DictValue& entry) {
        const std::string log_str = entry.DebugString();
        EXPECT_THAT(log_str, HasSubstr("Evaluating Autofill fetch plan"));
        EXPECT_THAT(log_str, HasSubstr("VehicleMake"));
        EXPECT_THAT(log_str, HasSubstr("BMW"));
        EXPECT_THAT(log_str, HasSubstr("EXACT"));

        EXPECT_THAT(log_str, HasSubstr("PassportNumber"));
        EXPECT_THAT(log_str, HasSubstr("\"data-pii\": \"true\""));
        EXPECT_THAT(log_str, HasSubstr("\"value\": \"ABC\""));
        EXPECT_THAT(log_str, HasSubstr("SUBSTRING"));
      })
      .WillRepeatedly(testing::Return());

  TestFuture<MemorySearchResults> future;
  service->Query(u"query", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());
  EXPECT_TRUE(future.Wait());

  log_router.UnregisterReceiver(&receiver);
}

// Tests that retrieved local data search results and subsequent filtered
// results are logged to autofill-internals logs.
TEST_F(AtMemoryQueryServiceTest, LogsLocalResultsWhenRetrieved) {
  LogRouter log_router;
  MockLogReceiver receiver;
  log_router.RegisterReceiver(&receiver);

  auto provider = std::make_unique<FakeMemoryDataProvider>();
  provider->SetResults({MemorySearchResult(MemoryDataType::kVehiclePlateNumber,
                                           u"Plate", u"12345")});

  auto service = CreateQueryService(std::move(provider), &log_router);

  AtMemoryQueryResponse response;
  response.set_query_classification(
      AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY);
  auto* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_NUMBER);

  StubFetchContextResponse(std::move(response));

  InSequence seq;
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                HasSubstr("Evaluating Autofill fetch plan"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                HasSubstr("Retrieved local data results (unfiltered)"));
    EXPECT_THAT(entry.DebugString(), HasSubstr("VehiclePlateNumber"));
    EXPECT_THAT(entry.DebugString(), HasSubstr("12345"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(), HasSubstr("Filtered local data results"));
    EXPECT_THAT(entry.DebugString(), HasSubstr("VehiclePlateNumber"));
    EXPECT_THAT(entry.DebugString(), HasSubstr("12345"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(), HasSubstr("Combined results:"));
    EXPECT_THAT(entry.DebugString(), HasSubstr("VehiclePlateNumber"));
    EXPECT_THAT(entry.DebugString(), HasSubstr("12345"));
  });

  TestFuture<MemorySearchResults> future;
  service->Query(u"query", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());
  EXPECT_TRUE(future.Wait());

  log_router.UnregisterReceiver(&receiver);
}

// Tests that discarded duplicate search results and their corresponding
// retained results are logged to autofill-internals logs.
TEST_F(AtMemoryQueryServiceTest, LogsDiscardedDuplicatesWhenFound) {
  LogRouter log_router;
  MockLogReceiver receiver;
  log_router.RegisterReceiver(&receiver);

  auto provider = std::make_unique<FakeMemoryDataProvider>();
  MemorySearchResult local_result(MemoryDataType::kVehiclePlateNumber, u"Plate",
                                  u"12345");
  local_result.is_local = true;
  local_result.sources = {MemoryEntrySource(MemoryEntrySourceType::kPhotos),
                          MemoryEntrySource(MemoryEntrySourceType::kGmail,
                                            "https://gmail.com/123")};
  provider->SetResults({local_result});

  auto service = CreateQueryService(std::move(provider), &log_router);

  AtMemoryQueryResponse response;
  response.set_query_classification(
      AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY);
  auto* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_NUMBER);

  auto* remote_result = response.add_results();
  auto* primary = remote_result->mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_NUMBER);
  primary->set_value("12345");

  StubFetchContextResponse(std::move(response));

  InSequence seq;
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                HasSubstr("Evaluating Autofill fetch plan"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                HasSubstr("Retrieved local data results (unfiltered)"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(), HasSubstr("Filtered local data results"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    const std::string log_str = entry.DebugString();
    EXPECT_THAT(log_str, HasSubstr("Discarded duplicate result"));
    EXPECT_THAT(log_str, HasSubstr("Retained result:"));
    EXPECT_THAT(log_str, HasSubstr("Discarded result:"));
    EXPECT_THAT(log_str, HasSubstr("VehiclePlateNumber"));
    EXPECT_THAT(log_str, HasSubstr("12345"));
    EXPECT_THAT(log_str, HasSubstr("PHOTOS"));
    EXPECT_THAT(log_str, HasSubstr("GMAIL (URL: https://gmail.com/123)"));
    EXPECT_THAT(log_str, HasSubstr("Is locally stored:"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(), HasSubstr("Combined results:"));
  });

  TestFuture<MemorySearchResults> future;
  service->Query(u"query", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());
  EXPECT_TRUE(future.Wait());

  log_router.UnregisterReceiver(&receiver);
}

// Tests that sensitive data in discarded duplicate logs is properly obfuscated.
TEST_F(AtMemoryQueryServiceTest,
       LogsDiscardedDuplicatesSensitiveDataObfuscated) {
  LogRouter log_router;
  MockLogReceiver receiver;
  log_router.RegisterReceiver(&receiver);

  auto provider = std::make_unique<FakeMemoryDataProvider>();
  MemorySearchResult local_result(MemoryDataType::kPassportNumber, u"Passport",
                                  u"PASS123");
  local_result.is_local = true;
  provider->SetResults({local_result});

  auto service = CreateQueryService(std::move(provider), &log_router);

  AtMemoryQueryResponse response;
  response.set_query_classification(
      AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY);
  auto* plan = response.mutable_autofill_fetch_plan();
  auto* spec = plan->add_fetch_specifications();
  spec->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);

  auto* remote_result = response.add_results();
  auto* primary = remote_result->mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);
  primary->set_value("PASS123");

  StubFetchContextResponse(std::move(response));

  InSequence seq;
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                HasSubstr("Evaluating Autofill fetch plan"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                HasSubstr("Retrieved local data results (unfiltered)"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(), HasSubstr("Filtered local data results"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    const std::string log_str = entry.DebugString();
    EXPECT_THAT(log_str, HasSubstr("Discarded duplicate result"));
    EXPECT_THAT(log_str, HasSubstr("\"data-pii\": \"true\""));
    EXPECT_THAT(log_str, HasSubstr("\"value\": \"PASS123\""));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(), HasSubstr("Combined results:"));
  });

  TestFuture<MemorySearchResults> future;
  service->Query(u"query", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());
  EXPECT_TRUE(future.Wait());

  log_router.UnregisterReceiver(&receiver);
}

// Tests that disambiguation metadata reordering is logged when the original
// metadata attribute order is changed.
TEST_F(AtMemoryQueryServiceTest,
       LogsDisambiguationReorderingWhenMetadataOrderChanged) {
  LogRouter log_router;
  MockLogReceiver receiver;
  log_router.RegisterReceiver(&receiver);

  std::unique_ptr<FakeMemoryDataProvider> provider =
      std::make_unique<FakeMemoryDataProvider>();
  MemorySearchResult local_result(MemoryDataType::kVehiclePlateNumber, u"Plate",
                                  u"CX100");
  local_result.is_local = true;
  // "Country" (shared) comes before "City" (unique).
  local_result.metadata_list = {
      EntryMetadata(MemoryDataType::kUnknown, u"Country", u"US"),
      EntryMetadata(MemoryDataType::kUnknown, u"City", u"NYC")};
  provider->SetResults({local_result});

  std::unique_ptr<AtMemoryQueryService> service =
      CreateQueryService(std::move(provider), &log_router);

  AtMemoryQueryResponse response;
  response.set_query_classification(
      AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY);
  AutofillFetchPlan* plan = response.mutable_autofill_fetch_plan();
  AutofillFetchSpecification* spec = plan->add_fetch_specifications();
  spec->set_data_type(
      personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_NUMBER);

  AtMemorySearchResult* remote_result = response.add_results();
  Attribute* primary = remote_result->mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_NUMBER);
  primary->set_value("CX200");
  Attribute* meta1 = remote_result->add_secondary_attributes();
  meta1->set_schemaless_key("Country");
  meta1->set_value("US");
  Attribute* meta2 = remote_result->add_secondary_attributes();
  meta2->set_schemaless_key("City");
  meta2->set_value("LA");

  StubFetchContextResponse(std::move(response));

  InSequence seq;
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                HasSubstr("Evaluating Autofill fetch plan"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                HasSubstr("Retrieved local data results (unfiltered)"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(), HasSubstr("Filtered local data results"));
  });
  // Disambiguation reorders metadata so "City" (freq 1) comes before "Country"
  // (freq 2). Both suggestions should log the reordering with their former
  // order before the final combined results are logged.
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(
        entry.DebugString(),
        ContainsRegex(
            R"(Reordering disambiguation metadata:[\s\S]*CX100[\s\S]*Country[\s\S]*City)"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(
        entry.DebugString(),
        ContainsRegex(
            R"(Reordering disambiguation metadata:[\s\S]*CX200[\s\S]*Country[\s\S]*City)"));
  });
  EXPECT_CALL(receiver, LogEntry).WillOnce([](const base::DictValue& entry) {
    EXPECT_THAT(entry.DebugString(),
                ContainsRegex(R"(Combined results:[\s\S]*CX100[\s\S]*CX200)"));
  });

  TestFuture<MemorySearchResults> future;
  service->Query(u"query", GURL("https://example.com"), u"Title",
                 future.GetRepeatingCallback());
  EXPECT_TRUE(future.Wait());

  log_router.UnregisterReceiver(&receiver);
}

}  // namespace

}  // namespace autofill
