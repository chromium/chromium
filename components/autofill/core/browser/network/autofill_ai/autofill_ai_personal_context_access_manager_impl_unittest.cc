// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/in_memory_entity_suppression_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager_impl_test_api.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/mock_personal_context_eligibility_service.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace autofill {

namespace {

using ::base::test::InvokeFuture;
using ::base::test::RunOnceCallback;
using personal_context::ContextMemoryError;
using ::personal_context::MockPersonalContextEligibilityService;
using ::personal_context::MockPersonalContextService;
using ::personal_context::proto::SensitivePiiPresence;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;
using ::testing::WithArg;

using RequestStatus = AutofillAiPersonalContextAccessManager::RequestStatus;

[[nodiscard]] auto HasAttributeWithValue(AttributeTypeName attribute_type_name,
                                         std::u16string value) {
  return Truly([=](const EntityInstance& entity) {
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(AttributeType(attribute_type_name));
    return attribute && attribute->GetCompleteInfo(/*app_locale=*/"") == value;
  });
}

// Checks that ContextMemoryAmbientAutofillRequest matches the `expected_types`
// and `expected_presence`.
MATCHER_P2(MatchContextFetchRequest, expected_types, expected_presence, "") {
  const auto& req = static_cast<
      const personal_context::proto::ContextMemoryAmbientAutofillRequest&>(arg);

  return req.return_spii_presence() == expected_presence &&
         ExplainMatchResult(UnorderedElementsAreArray(expected_types),
                            req.requested_types(), result_listener);
}

// Checks that ContextMemoryAmbientAutofillRequest matches the `expected_types`,
// `expected_presence`, and `expected_client_id`.
MATCHER_P3(MatchContextFetchRequestWithClientId,
           expected_types,
           expected_presence,
           expected_client_id,
           "") {
  const auto& req = static_cast<
      const personal_context::proto::ContextMemoryAmbientAutofillRequest&>(arg);

  return req.return_spii_presence() == expected_presence &&
         req.client_id() == expected_client_id &&
         ExplainMatchResult(UnorderedElementsAreArray(expected_types),
                            req.requested_types(), result_listener);
}

// Checks that an Entity proto has an encrypted entity payload matching
// `expected_encrypted_bytes`.
MATCHER_P(MatchEncryptedEntity, expected_encrypted_bytes, "") {
  return arg.entity_case() ==
             personal_context::proto::Entity::kEncryptedEntity &&
         arg.encrypted_entity() == expected_encrypted_bytes;
}

personal_context::FetchContextResult FetchContextSuccess(
    const personal_context::proto::ContextMemoryAmbientAutofillResponse&
        response) {
  personal_context::proto::Any any;
  response.SerializeToString(any.mutable_value());
  return personal_context::FetchContextResult(base::ok(std::move(any)));
}

template <size_t I = 0, typename T>
auto SaveOptSpanToVector(std::vector<T>* vector_ptr) {
  return [vector_ptr](auto&&... args) {
    auto opt_span = std::get<I>(
        std::forward_as_tuple(std::forward<decltype(args)>(args)...));
    if (opt_span.has_value()) {
      vector_ptr->assign(opt_span->begin(), opt_span->end());
    } else {
      vector_ptr->clear();
    }
  };
}

class MockAutofillAiPersonalContextAccessManagerObserver
    : public AutofillAiPersonalContextAccessManager::Observer {
 public:
  MockAutofillAiPersonalContextAccessManagerObserver() = default;
  ~MockAutofillAiPersonalContextAccessManagerObserver() override = default;

  MOCK_METHOD(void,
              OnPrefetchContextComplete,
              (const AutofillAiPersonalContextAccessManager& manager,
               std::optional<base::span<const EntityInstance>> entities),
              (override));
  MOCK_METHOD(void,
              OnMaskedEntityTypeEvicted,
              (const AutofillAiPersonalContextAccessManager& manager,
               EntityType type),
              (override));
};

class AutofillAiPersonalContextAccessManagerImplTest : public testing::Test {
 public:
  AutofillAiPersonalContextAccessManagerImplTest() {
    personal_context::prefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterIntegerPref(
        subscription_eligibility::prefs::kAiSubscriptionTier, 0);
    subscription_eligibility_service_ = std::make_unique<
        subscription_eligibility::SubscriptionEligibilityService>(
        &pref_service_);
    access_manager_ =
        std::make_unique<AutofillAiPersonalContextAccessManagerImpl>(
            &mock_personal_context_service_, &mock_eligibility_service_,
            subscription_eligibility_service_.get(), &pref_service_,
            &fake_device_info_sync_service_, &suppression_manager_);
    ON_CALL(mock_eligibility_service_, GetEligibilityState)
        .WillByDefault(Return(
            personal_context::PersonalContextEligibilityState::kEligible));
    ON_CALL(mock_eligibility_service_, GetNonEligibilityReason)
        .WillByDefault(Return(
            personal_context::PersonalContextNonEligibilityReason::kEligible));
    observation_.Observe(access_manager_.get());
  }
  ~AutofillAiPersonalContextAccessManagerImplTest() override = default;

  AutofillAiPersonalContextAccessManagerImpl& access_manager() {
    return *access_manager_;
  }

  EntitySuppressionManager& suppression_manager() {
    return suppression_manager_;
  }

  syncer::FakeDeviceInfoSyncService& fake_device_info_sync_service() {
    return fake_device_info_sync_service_;
  }

  MockPersonalContextService& mock_personal_context_service() {
    return mock_personal_context_service_;
  }

  MockPersonalContextEligibilityService& mock_eligibility_service() {
    return mock_eligibility_service_;
  }

  MockAutofillAiPersonalContextAccessManagerObserver& mock_observer() {
    return mock_observer_;
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  std::optional<EntityInstance> GetUnmaskedSpiiEntitySync(
      const EntityInstance::EntityId& id) {
    base::test::TestFuture<std::optional<EntityInstance>> future;
    access_manager().GetUnmaskedSpiiEntity(id, future.GetCallback());
    return future.Get();
  }

  // Prefetches personal context for the `requested_types`.
  //
  // Parameters:
  // - `requested_types`: The list of entity types to prefetch.
  // - `expected_spii_types`: The subset of `requested_types` that are sensitive
  //   PII (SPII) and expected to be requested from the backend.
  // - `non_spii_and_presence_response`: The mocked response for the main
  //   prefetch request (containing non-SPII entities fully and presence for
  //   any requested SPII types).
  // - `spii_response`: The mocked response for the subsequent SPII-specific
  //   request, if any SPII types are expected.
  void PrefetchContextSync(
      DenseSet<EntityType> requested_types,
      DenseSet<EntityType> expected_spii_types,
      const personal_context::proto::ContextMemoryAmbientAutofillResponse&
          non_spii_and_presence_response,
      const personal_context::proto::ContextMemoryAmbientAutofillResponse&
          spii_response = {}) {
    std::vector<personal_context::proto::EntityType> proto_types;
    for (EntityType type : requested_types) {
      if (!access_manager().IsTypePrefetched(type)) {
        proto_types.push_back(
            AutofillEntityTypeToPersonalContextEntityType(type));
      }
    }

    std::vector<personal_context::proto::EntityType> proto_spii_types;
    for (EntityType type : expected_spii_types) {
      proto_spii_types.push_back(
          AutofillEntityTypeToPersonalContextEntityType(type));
    }

    const bool has_spii = !expected_spii_types.empty();

    {
      InSequence s;

      EXPECT_CALL(
          mock_personal_context_service(),
          FetchContext(
              personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
              MatchContextFetchRequest(proto_types, has_spii), _, _))
          .WillOnce(RunOnceCallback<3>(
              FetchContextSuccess(non_spii_and_presence_response)));

      if (has_spii) {
        EXPECT_CALL(
            mock_personal_context_service(),
            FetchContext(personal_context::proto::
                             CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                         MatchContextFetchRequest(proto_spii_types, false), _,
                         _))
            .WillOnce(
                RunOnceCallback<3>(FetchContextSuccess(spii_response)));
      }
    }

    access_manager().PrefetchContext(requested_types);
  }

  // Prefetches a single masked Passport entity and returns its GUID.
  EntityInstance::EntityId PrefetchMaskedPassportAndGetGuid(
      std::string_view passport_number = "P123") {
    personal_context::proto::ContextMemoryAmbientAutofillResponse
        presence_response;
    presence_response.add_entities()
        ->mutable_sensitive_pii_presence()
        ->set_type(SensitivePiiPresence::PASSPORT);
    personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
    spii_response.add_entities()->mutable_passport()->set_number(
        std::string(passport_number));

    std::vector<EntityInstance> entities;
    EXPECT_CALL(mock_observer(),
                OnPrefetchContextComplete(_, Optional(IsEmpty())));
    EXPECT_CALL(mock_observer(),
                OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
        .WillOnce(SaveOptSpanToVector<1>(&entities));
    PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                        {EntityType(EntityTypeName::kPassport)},
                        presence_response, spii_response);
    CHECK_EQ(entities.size(), 1u);
    return entities[0].guid();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  TestingPrefServiceSimple pref_service_;

 private:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillAmbientAutofill};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockPersonalContextService mock_personal_context_service_;
  MockPersonalContextEligibilityService mock_eligibility_service_;
  std::unique_ptr<subscription_eligibility::SubscriptionEligibilityService>
      subscription_eligibility_service_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  InMemoryEntitySuppressionManager suppression_manager_;
  std::unique_ptr<AutofillAiPersonalContextAccessManagerImpl> access_manager_;
  MockAutofillAiPersonalContextAccessManagerObserver mock_observer_;
  base::ScopedObservation<AutofillAiPersonalContextAccessManagerImpl,
                          MockAutofillAiPersonalContextAccessManagerObserver>
      observation_{&mock_observer_};
};

// Tests that PrefetchContext successfully requests context from the backend and
// parses the returned entities, notifying observers about the result.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest, PrefetchContextSuccess) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_order()->set_order_id("12345");
  entity->mutable_order()->set_merchant_name("Amazon");

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(), OnPrefetchContextComplete)
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync(requested_types, {}, expected_response);

  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));
  EXPECT_THAT(entities,
              UnorderedElementsAre(AllOf(
                  Property(&EntityInstance::type,
                           Property(&EntityType::name, EntityTypeName::kOrder)),
                  HasAttributeWithValue(AttributeTypeName::kOrderId, u"12345"),
                  HasAttributeWithValue(AttributeTypeName::kOrderMerchantName,
                                        u"Amazon"))));
}

// Tests that PrefetchContext ignores entity types in the response that were
// not requested.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContextFiltersUnrequestedTypes) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  // Add the requested kOrder entity.
  personal_context::proto::Entity* order_entity = response.add_entities();
  order_entity->mutable_order()->set_order_id("12345");
  order_entity->mutable_order()->set_merchant_name("Amazon");

  // Add an unrequested kPassport presence signal.
  personal_context::proto::Entity* passport_presence = response.add_entities();
  passport_presence->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(), OnPrefetchContextComplete)
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync({EntityType(EntityTypeName::kOrder)}, {}, response);

  // The requested type should be marked as prefetched.
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));

  // The unrequested kPassport presence signal should NOT be cached.
  EXPECT_FALSE(
      test_api(access_manager())
          .IsPresenceSignalCached(EntityType(EntityTypeName::kPassport)));

  // The returned entities should only contain the requested kOrder.
  EXPECT_THAT(entities,
              UnorderedElementsAre(AllOf(
                  Property(&EntityInstance::type,
                           Property(&EntityType::name, EntityTypeName::kOrder)),
                  HasAttributeWithValue(AttributeTypeName::kOrderId, u"12345"),
                  HasAttributeWithValue(AttributeTypeName::kOrderMerchantName,
                                        u"Amazon"))));
}

// Tests that PrefetchContext filters out and only requests entity types that
// don't have a valid prefetching result available.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContextOnlyRequestsUnfetchedTypes) {
  // 1. First, prefetch Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_presence_response;
  passport_presence_response.add_entities()
      ->mutable_sensitive_pii_presence()
      ->set_type(SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_spii_response;
  passport_spii_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      passport_presence_response, passport_spii_response);
  ASSERT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // 2. Now call PrefetchContext for both Passport and Driver's
  // License. It should only request Driver's License.
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport),
      EntityType(EntityTypeName::kDriversLicense)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  expected_response.add_entities()->mutable_drivers_license()->set_number(
      "DL98765");

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::DRIVERS_LICENSE);

  PrefetchContextSync(requested_types,
                      {EntityType(EntityTypeName::kDriversLicense)},
                      presence_response, expected_response);

  // Both should now be prefetched.
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  EXPECT_TRUE(access_manager().IsTypePrefetched(
      EntityType(EntityTypeName::kDriversLicense)));
}

// Tests that PrefetchContext immediately returns and triggers no network
// requests when all requested entity types are already prefetched.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContextAllPrefetchedNoRequest) {
  // 1. Prefetch Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  ASSERT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // 2. Call PrefetchContext for Passport.
  // No network request should be made.
  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);

  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};
  access_manager().PrefetchContext(requested_types);
}

// Tests that PrefetchContext does not mark types as prefetched when the fetch
// context request fails.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest, PrefetchContextFailure) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::unexpected(expected_error))));
  EXPECT_CALL(mock_observer(), OnPrefetchContextComplete(_, Eq(std::nullopt)));
  access_manager().PrefetchContext(requested_types);
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));
}

// Tests that trigger results (e.g. initiated, skipped due to fresh cache,
// skipped due to backoff, skipped due to pending request in-flight) are
// correctly logged.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContextTriggerResultLogging) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  // Initial Prefetch (Cache Empty)
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_order()->set_order_id("12345");

  PrefetchContextSync(requested_types, {}, expected_response);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.PersonalContext.Prefetch.TriggerResult",
      PersonalContextPrefetchTriggerResult::kInitiated, 1);

  // Repeat Prefetch (Cache Fresh)
  access_manager().PrefetchContext(requested_types);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.Prefetch.TriggerResult",
      PersonalContextPrefetchTriggerResult::kSkippedFreshCache, 1);

  // Cache TTL Expired
  FastForwardBy(base::Minutes(31));

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(
          RunOnceCallback<3>(FetchContextSuccess(expected_response)));

  access_manager().PrefetchContext(requested_types);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.Prefetch.TriggerResult",
      PersonalContextPrefetchTriggerResult::kInitiated, 2);

  // Skipped in Backoff (After Failure)
  FastForwardBy(base::Minutes(31));

  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::unexpected(expected_error))));

  access_manager().PrefetchContext(requested_types);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.Prefetch.TriggerResult",
      PersonalContextPrefetchTriggerResult::kInitiated, 3);

  access_manager().PrefetchContext(requested_types);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.Prefetch.TriggerResult",
      PersonalContextPrefetchTriggerResult::kSkippedBackoff, 1);

  // Skipped In-Flight (Request Pending)
  // Fast forward out of backoff first to allow a new request to be initiated.
  FastForwardBy(base::Minutes(31));

  // Trigger a prefetch but do not complete the mock callback. This puts the
  // request state in RequestStatus::kPending.
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _));
  access_manager().PrefetchContext(requested_types);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.Prefetch.TriggerResult",
      PersonalContextPrefetchTriggerResult::kInitiated, 4);

  // Call again while the previous request is still pending.
  access_manager().PrefetchContext(requested_types);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.Prefetch.TriggerResult",
      PersonalContextPrefetchTriggerResult::kSkippedInFlight, 1);
}

// Tests that trigger results are only logged once per call to PrefetchContext,
// even if multiple requested types yield the same result.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContextTriggerResultLoggingMultipleTypes) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_order()->set_order_id("12345");

  // Since both types are initiated, we should only log `kInitiated` once.
  PrefetchContextSync({EntityType(EntityTypeName::kOrder),
                       EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      expected_response, expected_response);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.PersonalContext.Prefetch.TriggerResult",
      PersonalContextPrefetchTriggerResult::kInitiated, 1);
}

// Tests that request latency and total prefetch latency for SPII entity types
// (which require two requests to complete) are correctly recorded.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchRequestLatencyLogging) {
  const EntityType passport_type(EntityTypeName::kPassport);

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("12345");

  base::OnceCallback<void(personal_context::FetchContextResult)>
      presence_callback;
  base::OnceCallback<void(personal_context::FetchContextResult)> spii_callback;

  // Expect the request for Non-SPII (masked) entities and SPII presence.
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          MatchContextFetchRequest(
              std::vector<personal_context::proto::EntityType>{
                  AutofillEntityTypeToPersonalContextEntityType(passport_type)},
              true),
          _, _))
      .WillOnce(MoveArg<3>(&presence_callback));

  // Expect the request to fetch SPII (masked) entities.
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          MatchContextFetchRequest(
              std::vector<personal_context::proto::EntityType>{
                  AutofillEntityTypeToPersonalContextEntityType(passport_type)},
              false),
          _, _))
      .WillOnce(MoveArg<3>(&spii_callback));

  // Trigger prefetch.
  access_manager().PrefetchContext({passport_type});

  // Fast forward by 100ms.
  FastForwardBy(base::Milliseconds(100));

  // Complete the Non-SPII and presence request.
  std::move(presence_callback).Run(FetchContextSuccess(presence_response));

  // Verify NonSpiiAndPresence latency is recorded.
  histogram_tester().ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.RequestLatency.PrefetchNonSpiiAndPresence",
      base::Milliseconds(100), 1);
  histogram_tester().ExpectTotalCount(
      "Autofill.Ai.PersonalContext.RequestLatency.PrefetchSpiiMasked", 0);

  // Fast forward by another 50ms (total 150ms).
  FastForwardBy(base::Milliseconds(50));

  // Complete the SPII masked data request.
  std::move(spii_callback).Run(FetchContextSuccess(spii_response));

  // Verify SpiiMasked latency is recorded.
  histogram_tester().ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.RequestLatency.PrefetchSpiiMasked",
      base::Milliseconds(150), 1);
  histogram_tester().ExpectTotalCount(
      "Autofill.Ai.PersonalContext.RequestLatency.PrefetchNonSpiiAndPresence",
      1);
}

// Tests that request latency for unmasking sensitive PII entities is correctly
// recorded.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntityRequestLatencyLogging) {
  // Prefetch passport.
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_passport()->set_number("P123");

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync(requested_types, requested_types, presence_response,
                      expected_response);

  ASSERT_EQ(entities.size(), 1u);
  EntityInstance::EntityId passport_guid = entities[0].guid();

  base::OnceCallback<void(personal_context::FetchPiiEntitiesResult)> callback;
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities(_, _, _))
      .WillOnce(MoveArg<2>(&callback));

  base::test::TestFuture<std::optional<EntityInstance>> future;
  access_manager().GetUnmaskedSpiiEntity(passport_guid, future.GetCallback());

  // Fast forward by 456ms.
  FastForwardBy(base::Milliseconds(456));

  personal_context::proto::FetchPiiEntitiesResponse pii_response;
  *pii_response.add_entities() = *entity;
  std::move(callback).Run(personal_context::FetchPiiEntitiesResult(
      base::ok(std::move(pii_response))));

  ASSERT_TRUE(future.Get().has_value());
  histogram_tester().ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.RequestLatency.SpiiUnmasking",
      base::Milliseconds(456), 1);
}

// Tests that total prefetch latency for Non-SPII entity types (which only
// require a single request) is correctly recorded.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchTotalLatencyLogging_NonSpii) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_order()->set_order_id("12345");

  base::OnceCallback<void(personal_context::FetchContextResult)> callback;
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(MoveArg<3>(&callback));

  // Trigger prefetch.
  access_manager().PrefetchContext(requested_types);

  // Fast forward by 123 milliseconds.
  base::TimeDelta latency = base::Milliseconds(123);
  FastForwardBy(latency);

  // Complete the request.
  std::move(callback).Run(FetchContextSuccess(expected_response));

  // Verify that the total latency is recorded to the histogram.
  histogram_tester().ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.Prefetch.TotalLatency.Order", latency, 1);
}

// Tests that total prefetch latency for SPII entity types (which require
// two requests to complete) is correctly recorded when the final request
// completes.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchTotalLatencyLogging_Spii) {
  const EntityType passport_type(EntityTypeName::kPassport);

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("12345");

  base::OnceCallback<void(personal_context::FetchContextResult)>
      presence_callback;
  base::OnceCallback<void(personal_context::FetchContextResult)> spii_callback;

  // Expect Request 1 (collects presence).
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          MatchContextFetchRequest(
              std::vector<personal_context::proto::EntityType>{
                  AutofillEntityTypeToPersonalContextEntityType(passport_type)},
              true),
          _, _))
      .WillOnce(MoveArg<3>(&presence_callback));

  // Expect Request 2 (collects actual SPII entities).
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          MatchContextFetchRequest(
              std::vector<personal_context::proto::EntityType>{
                  AutofillEntityTypeToPersonalContextEntityType(passport_type)},
              false),
          _, _))
      .WillOnce(MoveArg<3>(&spii_callback));

  // Trigger prefetch.
  access_manager().PrefetchContext({passport_type});

  // Fast forward by 100ms.
  FastForwardBy(base::Milliseconds(100));

  // Complete Request 1 (collects presence signals).
  std::move(presence_callback).Run(FetchContextSuccess(presence_response));

  // Verify NonSpiiAndPresence request latency is recorded.
  histogram_tester().ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.RequestLatency.PrefetchNonSpiiAndPresence",
      base::Milliseconds(100), 1);
  // Total latency is not logged yet.
  histogram_tester().ExpectTotalCount(
      "Autofill.Ai.PersonalContext.Prefetch.TotalLatency.Passport", 0);

  // Fast forward by another 50ms (total 150ms).
  FastForwardBy(base::Milliseconds(50));

  // Complete Request 2 (collects actual entities).
  std::move(spii_callback).Run(FetchContextSuccess(spii_response));

  // Verify SpiiMasked request latency is recorded.
  histogram_tester().ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.RequestLatency.PrefetchSpiiMasked",
      base::Milliseconds(150), 1);
  // Verify that the total prefetch latency (150ms) is recorded.
  histogram_tester().ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.Prefetch.TotalLatency.Passport",
      base::Milliseconds(150), 1);
}

// Tests that request latency and total prefetch latency for Non-SPII entity
// types (which only require a single request) are correctly recorded.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchLatencyLogging_NonSpii) {
  base::HistogramTester histogram_tester;
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_order()->set_order_id("12345");

  base::OnceCallback<void(personal_context::FetchContextResult)> callback;
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(MoveArg<3>(&callback));

  // Trigger prefetch.
  access_manager().PrefetchContext(requested_types);

  // Fast forward by 123 milliseconds.
  base::TimeDelta latency = base::Milliseconds(123);
  FastForwardBy(latency);

  // Complete the request.
  std::move(callback).Run(FetchContextSuccess(expected_response));

  // Verify NonSpiiAndPresence request latency is recorded.
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.RequestLatency.PrefetchNonSpiiAndPresence",
      latency, 1);
  // Verify that the total latency is recorded to the histogram.
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.Ai.PersonalContext.Prefetch.TotalLatency.Order", latency, 1);
}

// Tests that PrefetchContext marks requested types as prefetched even when the
// response is empty.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContextEmptyResponse) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder),
      EntityType(EntityTypeName::kPassport)};

  // Empty response.
  personal_context::proto::ContextMemoryAmbientAutofillResponse empty_response;
  PrefetchContextSync(requested_types, {EntityType(EntityTypeName::kPassport)},
                      empty_response, empty_response);

  // Both types should be marked as prefetched.
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
}

// Tests that prefetched entities are evicted with a 30-minute TTL, and that the
// TTL is tracked per entity type.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest, PrefetchedEntities_TTL) {
  // 1. Prefetch Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_presence_response;
  passport_presence_response.add_entities()
      ->mutable_sensitive_pii_presence()
      ->set_type(SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      passport_spii_response;
  passport_spii_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      passport_presence_response, passport_spii_response);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  EXPECT_FALSE(access_manager().IsTypePrefetched(
      EntityType(EntityTypeName::kDriversLicense)));

  // Fast forward 15 minutes (Passport still valid).
  FastForwardBy(base::Minutes(15));
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // 2. Prefetch DL at T+15.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      dl_presence_response;
  dl_presence_response.add_entities()
      ->mutable_sensitive_pii_presence()
      ->set_type(SensitivePiiPresence::DRIVERS_LICENSE);
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      dl_spii_response;
  dl_spii_response.add_entities()->mutable_drivers_license()->set_number(
      "DL987");
  PrefetchContextSync({EntityType(EntityTypeName::kDriversLicense)},
                      {EntityType(EntityTypeName::kDriversLicense)},
                      dl_presence_response, dl_spii_response);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  EXPECT_TRUE(access_manager().IsTypePrefetched(
      EntityType(EntityTypeName::kDriversLicense)));

  // Fast forward another 15 minutes (Total T+30). Passport should expire, DL
  // should be valid.
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  EXPECT_TRUE(access_manager().IsTypePrefetched(
      EntityType(EntityTypeName::kDriversLicense)));

  // Fast forward another 15 minutes (Total T+45). DL should expire.
  EXPECT_CALL(mock_observer(),
              OnMaskedEntityTypeEvicted(
                  _, EntityType(EntityTypeName::kDriversLicense)));
  FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(access_manager().IsTypePrefetched(
      EntityType(EntityTypeName::kDriversLicense)));
}

// Tests that a follow-up prefetch request for an already prefetched type
// does nothing, and the original eviction timer correctly clears the cache
// when it expires.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContext_FollowUpRequestNoOp) {
  // 1. Prefetch Passport at T = 0.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // Fast forward 20 minutes (Passport still valid).
  FastForwardBy(base::Minutes(20));
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // 2. Trigger a follow-up prefetch request for Passport at T = 20.
  // Since the cache is still valid, no network request should be made.
  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);
  access_manager().PrefetchContext({EntityType(EntityTypeName::kPassport)});

  // Fast forward another 15 minutes (Total T = 35, past the original 30-min
  // TTL). The original eviction task should have fired at T = 30 and cleared
  // the cache.
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
}

// Tests that unmasked SPII entities are cached with a 1-minute TTL.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       CacheUnmaskedSpiiEntity_TTL) {
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);

  // Fast forward 30 seconds (still valid).
  FastForwardBy(base::Seconds(30));
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);

  // Fast forward another 31 seconds (expired).
  FastForwardBy(base::Seconds(31));
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), std::nullopt);
}

// Tests that presence signals are cached with a
// kPrefetchedEntitiesAndSignalsCacheTTL TTL.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       CachePresenceSignal_TTL) {
  const EntityType passport_type(EntityTypeName::kPassport);

  test_api(access_manager()).CachePresenceSignal(passport_type);
  EXPECT_TRUE(test_api(access_manager()).IsPresenceSignalCached(passport_type));

  // Fast forward 15 minutes (still valid).
  FastForwardBy(base::Minutes(15));
  EXPECT_TRUE(test_api(access_manager()).IsPresenceSignalCached(passport_type));

  // Fast forward another 16 minutes (expired).
  FastForwardBy(base::Minutes(16));
  EXPECT_FALSE(
      test_api(access_manager()).IsPresenceSignalCached(passport_type));
}

// Tests that ServerHasSpiiPresenceSignal returns true if presence signals are
// cached for a type.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       ServerHasSpiiPresenceSignal) {
  const EntityType passport_type(EntityTypeName::kPassport);
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  // 1. Initially, no data is available.
  EXPECT_FALSE(access_manager().ServerHasSpiiPresenceSignal(passport_type));

  // 2. Presence signal cached.
  test_api(access_manager()).CachePresenceSignal(passport_type);
  EXPECT_TRUE(access_manager().ServerHasSpiiPresenceSignal(passport_type));
}

// Tests that ServerHasSpiiPresenceSignal remains true even after the masked
// entity was unmasked (fetched).
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       ServerHasSpiiPresenceSignal_TrueAfterUnmasking) {
  const EntityType passport_type(EntityTypeName::kPassport);
  // 1. Prefetch (masked) Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync({passport_type}, {passport_type}, presence_response,
                      spii_response);
  ASSERT_TRUE(access_manager().IsTypePrefetched(passport_type));

  // Server should have data available after prefetch (presence signal cached).
  EXPECT_TRUE(access_manager().ServerHasSpiiPresenceSignal(passport_type));
}

// Tests that if the masked SPII response finishes first (which populates the
// prefetch cache and sets the status to Success), a subsequent presence signal
// response (from the first request) still correctly caches the presence signal,
// so ServerHasSpiiPresenceSignal() returns true.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PresenceResponseAfterSpiiResponsePopulatesPresenceCache) {
  const EntityType passport_type(EntityTypeName::kPassport);

  base::test::TestFuture<personal_context::FetchContextCallback>
      presence_callback_future;
  base::test::TestFuture<personal_context::FetchContextCallback>
      spii_callback_future;

  std::vector<personal_context::proto::EntityType> expected_types = {
      personal_context::proto::EntityType::PASSPORT};

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          MatchContextFetchRequest(expected_types, true), _, _))
      .WillOnce(WithArg<3>(InvokeFuture(presence_callback_future)));

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          MatchContextFetchRequest(expected_types, false), _, _))
      .WillOnce(WithArg<3>(InvokeFuture(spii_callback_future)));

  // 1. Trigger the prefetch.
  access_manager().PrefetchContext({passport_type});

  // 2. Mock responses.
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);

  // 3. Complete SPII request (Request 2) first.
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))));
  spii_callback_future.Take().Run(FetchContextSuccess(spii_response));

  EXPECT_TRUE(access_manager().IsTypePrefetched(passport_type));

  // 4. Complete Presence request (Request 1) second.
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  presence_callback_future.Take().Run(
      FetchContextSuccess(presence_response));

  // `ServerHasSpiiPresenceSignal` should now return true even if the presence
  // signal arrived after the SPII data was cached.
  EXPECT_TRUE(access_manager().ServerHasSpiiPresenceSignal(passport_type));
}

// Tests that resetting the state for a type evicts any existing prefetched
// entities of that type.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest, ResetStateForType) {
  // Prefetch passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // Reset the prefetch state. Should evict the passport.
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  test_api(access_manager())
      .ResetStateForType(EntityType(EntityTypeName::kPassport));
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
}

// Tests that natural expiration of the prefetched state also evicts any
// corresponding unmasked SPII entities, even if they haven't reached their
// individual TTL yet.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchedEntities_ExpirationResetsUnmaskedCache) {
  // 1. Prefetch a (masked) Passport at T=0.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  ASSERT_EQ(entities.size(), 1u);
  EntityInstance::EntityId passport_guid = entities[0].guid();

  // 2. Fast forward 29.5 minutes.
  FastForwardBy(base::Minutes(29) + base::Seconds(30));

  // The prefetched state is still valid (expires in 30 seconds).
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // 3. Cache unmasked SPII Passport at T=29.5.
  EntityInstance passport_unmasked = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  // Use the same GUID as the masked one to ensure they are linked.
  passport_unmasked = passport_unmasked.CopyWithNewEntityId(passport_guid);
  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport_unmasked);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), passport_unmasked);

  // 4. Fast forward 30 seconds (Total T+30). The prefetched entity expires.
  // This should also trigger the eviction of the unmasked SPII cache.
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), std::nullopt);
}

// Tests that GetUnmaskedSpiiEntity returns the cached entity immediately
// without calling the service if it is already in the unmasked cache.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_CacheHit) {
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport);

  // No service call expected.
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities).Times(0);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.Unmask.Result.PersonalContext",
      AutofillAiUnmaskResult::kCacheHit, 1);
}

// Tests that GetUnmaskedSpiiEntity triggers a service call on cache miss
// (when the masked entity is prefetched), caches the unmasked result,
// and returns it.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_CacheMiss_Success) {
  // 1. Prefetch (masked) Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  ASSERT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  ASSERT_EQ(entities.size(), 1u);
  EntityInstance passport_masked = entities[0];

  // 2. Prepare unmasked response.
  personal_context::proto::FetchPiiEntitiesResponse expected_response;
  expected_response.add_entities()->mutable_passport()->set_number(
      "P123_UNMASKED");

  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities(_, _, _))
      .WillOnce(RunOnceCallback<2>(personal_context::FetchPiiEntitiesResult(
          base::ok(std::move(expected_response)))));

  // Call GetUnmaskedSpiiEntity.
  {
    std::optional<EntityInstance> result =
        GetUnmaskedSpiiEntitySync(passport_masked.guid());
    ASSERT_TRUE(result.has_value());
    // The result should be unmasked and have the same GUID.
    EXPECT_FALSE(result->IsMaskedEntity());
    EXPECT_EQ(result->guid(), passport_masked.guid());
    EXPECT_EQ(
        result->attribute(AttributeType(AttributeTypeName::kPassportNumber))
            ->GetCompleteRawInfo(),
        u"P123_UNMASKED");
  }
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.Unmask.Result.PersonalContext",
      AutofillAiUnmaskResult::kSuccess, 1);

  // Verify that the unmasked passport is now cached in the unmasked cache by
  // calling `GetUnmaskedSpiiEntitySync` again and ensuring no service call
  // is made.
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities).Times(0);
  {
    std::optional<EntityInstance> cached_result =
        GetUnmaskedSpiiEntitySync(passport_masked.guid());
    ASSERT_TRUE(cached_result.has_value());
    EXPECT_EQ(cached_result->guid(), passport_masked.guid());
    EXPECT_FALSE(cached_result->IsMaskedEntity());
  }
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.Unmask.Result.PersonalContext",
      AutofillAiUnmaskResult::kCacheHit, 1);
}

// Tests that `GetUnmaskedSpiiEntity` returns `std::nullopt` immediately
// if the requested entity is not prefetched.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_NotPrefetched) {
  EntityInstance::EntityId unknown_id("unknown_id");

  // No service call expected because it's not prefetched.
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities).Times(0);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(unknown_id), std::nullopt);
  histogram_tester().ExpectTotalCount(
      "Autofill.Ai.Unmask.Result.PersonalContext", 0);
}

// Tests that `GetUnmaskedSpiiEntity` returns `std::nullopt` if the service call
// fails.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_ServiceFailure) {
  // 1. Prefetch (masked) Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  ASSERT_EQ(entities.size(), 1u);
  EntityInstance::EntityId passport_guid = entities[0].guid();

  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);

  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities(_, _, _))
      .WillOnce(RunOnceCallback<2>(personal_context::FetchPiiEntitiesResult(
          base::unexpected(expected_error))));

  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), std::nullopt);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.Unmask.Result.PersonalContext",
      AutofillAiUnmaskResult::kNetworkError, 1);
}

// Tests that when the service call returns a 200 OK with no entities,
// kEmptyResponse is logged to the Unmask.Result metric.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_EmptyResponse) {
  EntityInstance::EntityId passport_guid = PrefetchMaskedPassportAndGetGuid();

  personal_context::proto::FetchPiiEntitiesResponse empty_response;

  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities(_, _, _))
      .WillOnce(RunOnceCallback<2>(personal_context::FetchPiiEntitiesResult(
          base::ok(std::move(empty_response)))));

  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), std::nullopt);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.Unmask.Result.PersonalContext",
      AutofillAiUnmaskResult::kEmptyResponse, 1);
}

// Tests that when the service call returns a ResponseParseError,
// kParsingError is logged to the Unmask.Result metric.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_ResponseParseError) {
  EntityInstance::EntityId passport_guid = PrefetchMaskedPassportAndGetGuid();

  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kResponseParseError);

  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities(_, _, _))
      .WillOnce(RunOnceCallback<2>(personal_context::FetchPiiEntitiesResult(
          base::unexpected(expected_error))));

  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), std::nullopt);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.Unmask.Result.PersonalContext",
      AutofillAiUnmaskResult::kParsingError, 1);
}

// Tests that when OnEligibilityStateChanged is called with a disabled state,
// all state is wiped.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest, WipeStateOnDisablement) {
  // 1. Prefetch a (masked) passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  ASSERT_EQ(entities.size(), 1u);
  EntityInstance::EntityId passport_guid = entities[0].guid();

  // 2. Call OnEligibilityStateChanged with an ENABLED state. State should not
  // be wiped.
  access_manager().OnEligibilityStateChanged(
      personal_context::PersonalContextEligibilityState::kEligible);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // 3. Call OnEligibilityStateChanged with a DISABLED state. State should be
  // wiped.
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  access_manager().OnEligibilityStateChanged(
      personal_context::PersonalContextEligibilityState::kDisabledNotEligible);
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
}

// Tests that a pending request blocks subsequent requests for the same type.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PendingRequestBlocksSubsequent) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  base::test::TestFuture<personal_context::FetchContextCallback> future;
  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(WithArg<3>(InvokeFuture(future)));

  // First request should trigger FetchContext.
  access_manager().PrefetchContext(requested_types);
  ASSERT_TRUE(future.IsReady());

  // Second request for the same type should NOT trigger FetchContext.
  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);
  access_manager().PrefetchContext(requested_types);

  // It isn't prefetched yet.
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));

  // Resolve the first request.
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  future.Take().Run(FetchContextSuccess(response));

  // Now it is prefetched.
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));
}

// Tests that failed requests trigger exponential backoff.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest, FailureTriggersBackoff) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    // 1. First failure.
    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
            _, _))
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::unexpected(expected_error))));
    EXPECT_CALL(check, Call("1. First failure"));

    // 2. Immediate retry should be blocked by backoff (1s delay).
    EXPECT_CALL(check, Call("2. Immediate retry"));

    // 3. Fast forward 500ms (still blocked).
    EXPECT_CALL(check, Call("3. Fast forward 500ms"));

    // 4. Fast forward another 500ms (total 1s, backoff expired).
    // Second failure.
    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
            _, _))
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::unexpected(expected_error))));
    EXPECT_CALL(check, Call("4. Second failure"));

    // 5. Immediate retry should be blocked by backoff (2s delay now).
    EXPECT_CALL(check, Call("5. Immediate retry"));

    // 6. Fast forward 1.5s (still blocked).
    EXPECT_CALL(check, Call("6. Fast forward 1.5s"));

    // 7. Fast forward another 500ms (total 2s, backoff expired).
    // This time it succeeds.
    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
            _, _))
        .WillOnce(RunOnceCallback<3>(FetchContextSuccess(response)));
    EXPECT_CALL(check, Call("7. Success"));

    // 9. Success resets failure count.
    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
            _, _))
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::unexpected(expected_error))));
    EXPECT_CALL(check, Call("9. Reset success"));
  }

  // 1. First failure.
  access_manager().PrefetchContext(requested_types);
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));
  check.Call("1. First failure");

  // 2. Immediate retry should be blocked by backoff (1s delay).
  access_manager().PrefetchContext(requested_types);
  check.Call("2. Immediate retry");

  // 3. Fast forward 500ms (still blocked).
  FastForwardBy(base::Milliseconds(500));
  access_manager().PrefetchContext(requested_types);
  check.Call("3. Fast forward 500ms");

  // 4. Fast forward another 500ms (total 1s, backoff expired).
  FastForwardBy(base::Milliseconds(500));
  access_manager().PrefetchContext(requested_types);
  check.Call("4. Second failure");

  // 5. Immediate retry should be blocked by backoff (2s delay now).
  access_manager().PrefetchContext(requested_types);
  check.Call("5. Immediate retry");

  // 6. Fast forward 1.5s (still blocked).
  FastForwardBy(base::Milliseconds(1500));
  access_manager().PrefetchContext(requested_types);
  check.Call("6. Fast forward 1.5s");

  // 7. Fast forward another 500ms (total 2s, backoff expired).
  FastForwardBy(base::Milliseconds(500));
  access_manager().PrefetchContext(requested_types);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));
  check.Call("7. Success");

  // 8. Expire the prefetched state (30 mins).
  FastForwardBy(base::Minutes(30));
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));

  // 9. Request again, should succeed immediately because failure count was
  // reset on success.
  access_manager().PrefetchContext(requested_types);
  check.Call("9. Reset success");
}

// Tests that the prefetch status transitions correctly (`kNotStarted` ->
// `kPending` -> `kSuccess` -> `kNotStarted`) and the observer is notified
// with success = true when a prefetch request succeeds.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchStatusAndObserverSuccess) {
  const EntityType order_type = EntityType(EntityTypeName::kOrder);

  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kNotStarted);

  base::test::TestFuture<personal_context::FetchContextCallback> future;
  EXPECT_CALL(mock_personal_context_service(), FetchContext)
      .WillOnce(WithArg<3>(InvokeFuture(future)));

  // 1. Start prefetch. Status should transition to `kPending`.
  access_manager().PrefetchContext({order_type});
  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kPending);

  // 2. Resolve request successfully. Status should transition to `kSuccess`,
  // and observer should be notified with success = true.
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;

  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  future.Take().Run(FetchContextSuccess(response));

  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kSuccess);

  // 3. Fast forward 30 minutes (TTL expires). Status should transition back to
  // `kNotStarted`.
  FastForwardBy(base::Minutes(30));
  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kNotStarted);
}

// Tests that the prefetch status transitions correctly (`kNotStarted` ->
// `kPending` -> `kFailure` -> `kNotStarted`) and the observer is notified
// with success = false when a prefetch request fails.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchStatusAndObserverFailure) {
  const EntityType order_type = EntityType(EntityTypeName::kOrder);

  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kNotStarted);

  base::test::TestFuture<personal_context::FetchContextCallback> future;
  EXPECT_CALL(mock_personal_context_service(), FetchContext)
      .WillOnce(WithArg<3>(InvokeFuture(future)));

  // 1. Start prefetch. Status should transition to `kPending`.
  access_manager().PrefetchContext({order_type});
  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kPending);

  // 2. Resolve request with failure. Status should transition to `kFailure`,
  // and observer should be notified with success = false.
  EXPECT_CALL(mock_observer(), OnPrefetchContextComplete(_, Eq(std::nullopt)));
  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);
  future.Take().Run(
      personal_context::FetchContextResult(base::unexpected(expected_error)));

  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kFailure);

  // 3. Wipe state. Status should transition back to `kNotStarted`.
  access_manager().OnEligibilityStateChanged(
      personal_context::PersonalContextEligibilityState::kDisabledNotEligible);
  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kNotStarted);
}

// Tests that calling PrefetchContext when all types are
// prefetched indeed notifies the observer synchronously.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchWhenAlreadyPrefetchedNotifiesObserver) {
  MockAutofillAiPersonalContextAccessManagerObserver observer;
  access_manager().AddObserver(&observer);

  // 1. Prefetch Passport.
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);

  // 2. Call Prefetch again. Expect observer to be notified synchronously.
  EXPECT_CALL(observer, OnPrefetchContextComplete(_, Optional(IsEmpty())));
  access_manager().PrefetchContext({EntityType(EntityTypeName::kPassport)});
}

// Tests that unmasked SPII entities are cached with a configurable TTL.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       CacheUnmaskedSpiiEntity_TTL_Configurable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAmbientAutofill,
      {{features::kAutofillAmbientAutofillUnmaskedSpiiCacheTTL.name, "2m"}});

  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);

  // Fast forward 90 seconds (still valid, past the default 1-min TTL).
  FastForwardBy(base::Seconds(90));
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);

  // Fast forward another 31 seconds (expired, Total T = 121s).
  FastForwardBy(base::Seconds(31));
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), std::nullopt);
}

// Tests that presence signals are cached with a configurable TTL.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       CachePresenceSignal_TTL_Configurable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAmbientAutofill,
      {{features::kAutofillAmbientAutofillPrefetchedEntitiesAndSignalsCacheTTL
            .name,
        "10m"}});

  const EntityType passport_type(EntityTypeName::kPassport);

  test_api(access_manager()).CachePresenceSignal(passport_type);
  EXPECT_TRUE(test_api(access_manager()).IsPresenceSignalCached(passport_type));

  // Fast forward 5 minutes (still valid).
  FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(test_api(access_manager()).IsPresenceSignalCached(passport_type));

  // Fast forward another 6 minutes (expired, Total T = 11m, past the 10m TTL).
  FastForwardBy(base::Minutes(6));
  EXPECT_FALSE(
      test_api(access_manager()).IsPresenceSignalCached(passport_type));
}

// Tests that PrefetchContext uses the configurable TTL for cache freshness.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContext_TTL_Configurable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAmbientAutofill,
      {{features::kAutofillAmbientAutofillPrefetchedEntitiesAndSignalsCacheTTL
            .name,
        "10m"}});

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");

  // 1. Initial prefetch at T = 0.
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // Fast forward 5 minutes (Passport still valid).
  FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // 2. Trigger a follow-up prefetch request for Passport at T = 5.
  // Since the cache is still valid, no network request should be made.
  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);
  access_manager().PrefetchContext({EntityType(EntityTypeName::kPassport)});

  // Fast forward another 6 minutes (Total T = 11, past the 10-min TTL).
  // The original eviction task should have fired at T = 10 and cleared the
  // cache.
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  FastForwardBy(base::Minutes(6));
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
}

// Tests that the state is reset when the personal context settings toggle is
// turned off.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       ResetAllStateOnTogglePrefChangedOff) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::ContextMemoryAmbientAutofillResponse spii_response;
  spii_response.add_entities()->mutable_passport()->set_number("P123");

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));
  PrefetchContextSync({EntityType(EntityTypeName::kPassport)},
                      {EntityType(EntityTypeName::kPassport)},
                      presence_response, spii_response);
  ASSERT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  ASSERT_EQ(entities.size(), 1u);

  // Set the toggle pref to false. This should trigger eviction.
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);

  // Verify that the state is wiped.
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
}

// Tests that `Autofill.Ai.PersonalContext.NonEligibilityReason` is logged after
// a 30-second startup delay.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       LogsAmbientNonEligibilityReasonAfterStartupDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAmbientAutofill,
        {{"ambient_autofill_eligible_tiers", "1,2"}}}},
      {});

  // Before 30 seconds, startup logging should not have occurred.
  histogram_tester().ExpectTotalCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason", 0);

  // Fast forward by 31 seconds to trigger startup logging.
  FastForwardBy(base::Seconds(31));

  histogram_tester().ExpectTotalCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason", 1);
}

// Tests that `Autofill.Ai.PersonalContext.NonEligibilityReason` is logged on
// subscription tier updates after the startup delay has elapsed.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       LogsAmbientNonEligibilityReasonOnTierChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAmbientAutofill,
        {{"ambient_autofill_eligible_tiers", "1,2"}}}},
      {});

  // Set tier to an eligible tier (1) before startup logging triggers.
  pref_service_.SetInteger(subscription_eligibility::prefs::kAiSubscriptionTier,
                           1);

  // Fast forward by 31 seconds to complete startup logging (records kEligible).
  FastForwardBy(base::Seconds(31));

  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 1);

  // Then change tier to an ineligible tier (99).
  pref_service_.SetInteger(subscription_eligibility::prefs::kAiSubscriptionTier,
                           99);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::
          kNotG1SubscriberOrAndroidPremiumDevice,
      1);
}

// Tests that `Autofill.Ai.PersonalContext.NonEligibilityReason` is logged on
// settings toggle updates after the startup delay has elapsed.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       LogsAmbientNonEligibilityReasonOnToggleChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAmbientAutofill,
        {{"ambient_autofill_eligible_tiers", "1,2"}}}},
      {});

  pref_service_.SetInteger(subscription_eligibility::prefs::kAiSubscriptionTier,
                           1);
  FastForwardBy(base::Seconds(31));

  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 1);

  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::
          kPersonalIntelligencePrefDisabled,
      1);

  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 2);
}

#if BUILDFLAG(IS_ANDROID)
// Tests that `Autofill.Ai.PersonalContext.NonEligibilityReason` logs
// `kEligible` when the Android device is supported, even if the user's
// subscription tier is not in the eligible tiers list.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       LogsAmbientEligibilityReasonOnAndroidPremiumDevice) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAmbientAutofill,
        {{"ambient_autofill_eligible_tiers", "1,2"},
         {"ambient_autofill_enabled_devices",
          base::SysInfo::HardwareModelName()}}}},
      {});

  // Set tier to an eligible tier (1) before startup delay.
  pref_service_.SetInteger(subscription_eligibility::prefs::kAiSubscriptionTier,
                           1);

  // Fast forward by 31 seconds to complete startup logging.
  FastForwardBy(base::Seconds(31));

  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 1);

  // Then change tier to an ineligible tier (99). Since the Android device is
  // supported, the user remains eligible (`kEligible`), so no duplicate sample
  // is logged.
  pref_service_.SetInteger(subscription_eligibility::prefs::kAiSubscriptionTier,
                           99);
  histogram_tester().ExpectBucketCount(
      "Autofill.Ai.PersonalContext.NonEligibilityReason",
      personal_context::PersonalContextNonEligibilityReason::kEligible, 1);
}
#endif

// Tests that `PrefetchContext` populates the `client_id` field of
// `ContextMemoryAmbientAutofillRequest` using the cache GUID retrieved from
// `DeviceInfoSyncService`.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContext_PopulatesClientIdFromCacheGuid) {
  const EntityType kOrderType = EntityType(EntityTypeName::kOrder);

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          MatchContextFetchRequestWithClientId(
              std::vector<personal_context::proto::EntityType>{
                  AutofillEntityTypeToPersonalContextEntityType(kOrderType)},
              /*expected_presence=*/false,
              /*expected_client_id=*/
              fake_device_info_sync_service()
                  .GetLocalDeviceInfoProvider()
                  ->GetLocalDeviceInfo()
                  ->guid()),
          _, _));

  access_manager().PrefetchContext({kOrderType});
}

// Tests that `PrefetchContext` populates an empty `client_id` when local device
// info is unavailable.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContext_EmptyClientIdWhenLocalDeviceInfoUnavailable) {
  fake_device_info_sync_service().GetLocalDeviceInfoProvider()->SetReady(false);

  const EntityType kOrderType = EntityType(EntityTypeName::kOrder);

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          MatchContextFetchRequestWithClientId(
              std::vector<personal_context::proto::EntityType>{
                  AutofillEntityTypeToPersonalContextEntityType(kOrderType)},
              /*expected_presence=*/false,
              /*expected_client_id=*/""),
          _, _));

  access_manager().PrefetchContext({kOrderType});
}

// Tests that prefetched personal context entities that are suppressed in the
// `EntitySuppressionManager` are filtered out before notifying observers.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       PrefetchContext_SuppressedEntitiesAreFiltered) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  response.add_entities()->mutable_order()->set_order_id("ORD1");
  response.add_entities()->mutable_order()->set_order_id("ORD2");

  suppression_manager().SuppressEntity(
      *PersonalContextEntityToEntityInstance(response.entities(0)));

  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(
                  _, Optional(ElementsAre(HasAttributeWithValue(
                         AttributeTypeName::kOrderId, u"ORD2")))));

  PrefetchContextSync({EntityType(EntityTypeName::kOrder)}, {}, response);
}

// Tests that suppressing an entity evicts cached masked entities and re-emits
// the remaining unsuppressed entities.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       OnEntitySuppressionsChanged_SuppressEntityEvictsAndReemits) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  response.add_entities()->mutable_order()->set_order_id("ORD1");
  response.add_entities()->mutable_order()->set_order_id("ORD2");

  EXPECT_CALL(mock_observer(), OnPrefetchContextComplete);
  PrefetchContextSync({EntityType(EntityTypeName::kOrder)}, {}, response);

  InSequence s;
  EXPECT_CALL(mock_observer(),
              OnMaskedEntityTypeEvicted(Ref(access_manager()),
                                        EntityType(EntityTypeName::kOrder)));
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(
                  _, Optional(ElementsAre(HasAttributeWithValue(
                         AttributeTypeName::kOrderId, u"ORD2")))));

  suppression_manager().SuppressEntity(
      *PersonalContextEntityToEntityInstance(response.entities(0)));
}

// Tests that unsuppressing an entity evicts cached masked entities and re-emits
// all newly unsuppressed entities.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       OnEntitySuppressionsChanged_UnsuppressEntityEvictsAndReemits) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  response.add_entities()->mutable_order()->set_order_id("ORD1");
  response.add_entities()->mutable_order()->set_order_id("ORD2");

  EntityInstance order1 =
      *PersonalContextEntityToEntityInstance(response.entities(0));
  suppression_manager().SuppressEntity(order1);

  EXPECT_CALL(mock_observer(), OnPrefetchContextComplete);
  PrefetchContextSync({EntityType(EntityTypeName::kOrder)}, {}, response);

  InSequence s;
  EXPECT_CALL(mock_observer(),
              OnMaskedEntityTypeEvicted(Ref(access_manager()),
                                        EntityType(EntityTypeName::kOrder)));
  EXPECT_CALL(
      mock_observer(),
      OnPrefetchContextComplete(
          _,
          Optional(UnorderedElementsAre(
              HasAttributeWithValue(AttributeTypeName::kOrderId, u"ORD1"),
              HasAttributeWithValue(AttributeTypeName::kOrderId, u"ORD2")))));

  suppression_manager().UnsuppressEntity(order1);
}

// Tests that OnEntitySuppressionsChanged is a no-op when the proto cache is
// empty.
TEST_F(AutofillAiPersonalContextAccessManagerImplTest,
       OnEntitySuppressionsChanged_EmptyCacheIsNoOp) {
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted).Times(0);
  EXPECT_CALL(mock_observer(), OnPrefetchContextComplete).Times(0);

  EntityInstance passport = test::GetPassportEntityInstance();
  suppression_manager().SuppressEntity(passport);
}

class AutofillAiPersonalContextAccessManagerImplSpiiCacheTest
    : public AutofillAiPersonalContextAccessManagerImplTest {
 public:
  AutofillAiPersonalContextAccessManagerImplSpiiCacheTest() = default;

  // Prefetches personal context for `requested_types` in a single request as
  // expected when `kAutofillAmbientAutofillSpiiCache` is enabled.
  void PrefetchContextSync(
      DenseSet<EntityType> requested_types,
      const personal_context::proto::ContextMemoryAmbientAutofillResponse&
          response) {
    std::vector<personal_context::proto::EntityType> proto_types;
    for (EntityType type : requested_types) {
      if (!access_manager().IsTypePrefetched(type)) {
        proto_types.push_back(
            AutofillEntityTypeToPersonalContextEntityType(type));
      }
    }

    if (proto_types.empty()) {
      access_manager().PrefetchContext(requested_types);
      return;
    }

    EXPECT_CALL(
        mock_personal_context_service(),
        FetchContext(
            personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
            MatchContextFetchRequest(proto_types, /*expected_presence=*/false),
            _, _))
        .WillOnce(RunOnceCallback<3>(FetchContextSuccess(response)));

    access_manager().PrefetchContext(requested_types);
  }

  // Helper to create an encrypted proto entity wrapper.
  personal_context::proto::Entity CreateEncryptedEntity(
      std::string_view encrypted_bytes) {
    personal_context::proto::Entity entity;
    entity.set_encrypted_entity(std::string(encrypted_bytes));
    return entity;
  }

  // Helper to create a decrypted passport proto entity.
  personal_context::proto::Entity CreateDecryptedPassportEntity(
      std::string_view passport_number,
      std::string_view passport_name) {
    personal_context::proto::Entity entity;
    entity.mutable_passport()->set_number(std::string(passport_number));
    entity.mutable_passport()->set_name(std::string(passport_name));
    return entity;
  }

  // Helper to create a decrypted drivers license proto entity.
  personal_context::proto::Entity CreateDecryptedDriversLicenseEntity(
      std::string_view dl_number,
      std::string_view dl_name) {
    personal_context::proto::Entity entity;
    entity.mutable_drivers_license()->set_number(std::string(dl_number));
    entity.mutable_drivers_license()->set_name(std::string(dl_name));
    return entity;
  }

  // Prefetches a single encrypted Passport entity and returns its GUID.
  EntityInstance::EntityId PrefetchEncryptedPassportAndGetGuid(
      std::string_view encrypted_bytes = "encrypted_passport_bytes",
      std::string_view passport_number = "P123",
      std::string_view passport_name = "John Doe") {
    personal_context::proto::ContextMemoryAmbientAutofillResponse response;
    *response.add_entities() = CreateEncryptedEntity(encrypted_bytes);

    EXPECT_CALL(mock_personal_context_service(),
                DecryptEntity(MatchEncryptedEntity(encrypted_bytes)))
        .WillOnce(Return(
            CreateDecryptedPassportEntity(passport_number, passport_name)));

    std::vector<EntityInstance> entities;
    EXPECT_CALL(mock_observer(),
                OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
        .WillOnce(SaveOptSpanToVector<1>(&entities));

    PrefetchContextSync({EntityType(EntityTypeName::kPassport)}, response);
    CHECK_EQ(entities.size(), 1u);
    return entities[0].guid();
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillAmbientAutofillSpiiCache};
};

// Tests that when `kAutofillAmbientAutofillSpiiCache` is enabled, prefetching
// SPII types sends a single request that does not ask for SPII presence, and
// directly marks the type as prefetched.
TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       PrefetchContext_SpiiTypesOnlySendsSingleRequest) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  *response.add_entities() = CreateEncryptedEntity("encrypted_passport_data");

  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("encrypted_passport_data")))
      .WillOnce(Return(CreateDecryptedPassportEntity("P12345", "Jane Doe")));

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));

  PrefetchContextSync(requested_types, response);

  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  ASSERT_EQ(entities.size(), 1u);
  EXPECT_TRUE(entities[0].IsMaskedEntity());
  EXPECT_THAT(
      entities,
      UnorderedElementsAre(AllOf(
          Property(&EntityInstance::type,
                   Property(&EntityType::name, EntityTypeName::kPassport)),
          HasAttributeWithValue(AttributeTypeName::kPassportName, u"Jane Doe"),
          HasAttributeWithValue(AttributeTypeName::kPassportNumber, u"45"))));
}

// Tests that prefetching a mix of non-SPII and SPII types sends a single
// request containing all types without requesting presence, and marks all types
// as prefetched.
TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       PrefetchContext_MixedTypesOnlySendsSingleRequest) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder),
      EntityType(EntityTypeName::kPassport)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  personal_context::proto::Entity* order_entity = response.add_entities();
  order_entity->mutable_order()->set_order_id("ORD-999");
  order_entity->mutable_order()->set_merchant_name("BestBuy");

  *response.add_entities() = CreateEncryptedEntity("encrypted_passport_bytes");

  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("encrypted_passport_bytes")))
      .WillOnce(Return(CreateDecryptedPassportEntity("P5678", "Alice")));

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));

  PrefetchContextSync(requested_types, response);

  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  ASSERT_EQ(entities.size(), 2u);
  EXPECT_THAT(
      entities,
      UnorderedElementsAre(
          AllOf(Property(&EntityInstance::type,
                         Property(&EntityType::name, EntityTypeName::kOrder)),
                HasAttributeWithValue(AttributeTypeName::kOrderId, u"ORD-999"),
                HasAttributeWithValue(AttributeTypeName::kOrderMerchantName,
                                      u"BestBuy")),
          AllOf(
              Property(&EntityInstance::type,
                       Property(&EntityType::name, EntityTypeName::kPassport)),
              HasAttributeWithValue(AttributeTypeName::kPassportName, u"Alice"),
              HasAttributeWithValue(AttributeTypeName::kPassportNumber,
                                    u"78"))));
}

// Tests that if decrypting an encrypted entity fails, the entity is dropped,
// but the requested type is still marked as prefetched.
TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       PrefetchContext_EncryptedEntityDecryptionFails) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  *response.add_entities() = CreateEncryptedEntity("corrupt_encrypted_data");

  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("corrupt_encrypted_data")))
      .WillOnce(Return(std::nullopt));

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())))
      .WillOnce(SaveOptSpanToVector<1>(&entities));

  PrefetchContextSync(requested_types, response);

  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  EXPECT_THAT(entities, IsEmpty());
}

// Tests that if the decrypted entity type was not in `requested_types`,
// it is filtered out and not returned to observers.
TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       PrefetchContext_FiltersUnrequestedDecryptedTypes) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  *response.add_entities() = CreateEncryptedEntity("encrypted_dl_data");

  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("encrypted_dl_data")))
      .WillOnce(Return(CreateDecryptedDriversLicenseEntity("DL12345", "Bob")));

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())))
      .WillOnce(SaveOptSpanToVector<1>(&entities));

  PrefetchContextSync(requested_types, response);

  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  EXPECT_FALSE(access_manager().IsTypePrefetched(
      EntityType(EntityTypeName::kDriversLicense)));
  EXPECT_THAT(entities, IsEmpty());
}

// Tests prefetching multiple encrypted entities in the same response.
TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       PrefetchContext_MultipleEncryptedEntities) {
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport),
      EntityType(EntityTypeName::kDriversLicense)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  *response.add_entities() = CreateEncryptedEntity("passport_enc_bytes");
  *response.add_entities() = CreateEncryptedEntity("dl_enc_bytes");

  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("passport_enc_bytes")))
      .WillOnce(Return(CreateDecryptedPassportEntity("P100", "John")));
  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("dl_enc_bytes")))
      .WillOnce(Return(CreateDecryptedDriversLicenseEntity("DL200", "John")));

  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));

  PrefetchContextSync(requested_types, response);

  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
  EXPECT_TRUE(access_manager().IsTypePrefetched(
      EntityType(EntityTypeName::kDriversLicense)));
  ASSERT_EQ(entities.size(), 2u);
  EXPECT_THAT(
      entities,
      UnorderedElementsAre(
          AllOf(
              Property(&EntityInstance::type,
                       Property(&EntityType::name, EntityTypeName::kPassport)),
              HasAttributeWithValue(AttributeTypeName::kPassportNumber, u"0")),
          AllOf(Property(&EntityInstance::type,
                         Property(&EntityType::name,
                                  EntityTypeName::kDriversLicense)),
                HasAttributeWithValue(AttributeTypeName::kDriversLicenseNumber,
                                      u"00"))));
}

// Tests that prefetched encrypted entities expire after the 30-minute TTL.
TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       PrefetchedEntities_TTL) {
  PrefetchEncryptedPassportAndGetGuid("enc_bytes", "P123", "John Doe");
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // Fast forward 15 minutes (still valid).
  FastForwardBy(base::Minutes(15));
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // Fast forward another 15 minutes (TTL expires at 30 min).
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
}

// Tests that GetUnmaskedSpiiEntity decrypts and unmasks the entity locally
// on the first call, and serves it from cache on subsequent calls when
// SpiiCache is enabled.
TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       GetUnmaskedSpiiEntity_SpiiCacheEnabled_Success) {
  EntityInstance::EntityId passport_guid =
      PrefetchEncryptedPassportAndGetGuid("enc_bytes", "P123", "John Doe");
  // GetUnmaskedSpiiEntity should call `DecryptEntity`.
  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("enc_bytes")))
      .WillOnce(Return(CreateDecryptedPassportEntity("P123", "John Doe")));

  // Call GetUnmaskedSpiiEntity.
  std::optional<EntityInstance> unmasked =
      GetUnmaskedSpiiEntitySync(passport_guid);
  ASSERT_TRUE(unmasked.has_value());
  EXPECT_FALSE(unmasked->IsMaskedEntity());
  EXPECT_THAT(
      *unmasked,
      AllOf(Property(&EntityInstance::type,
                     Property(&EntityType::name, EntityTypeName::kPassport)),
            HasAttributeWithValue(AttributeTypeName::kPassportNumber, u"P123"),
            HasAttributeWithValue(AttributeTypeName::kPassportName,
                                  u"John Doe")));

  // Subsequent call should be a cache hit(no DecryptEntity call expected).
  std::optional<EntityInstance> cached_unmasked =
      GetUnmaskedSpiiEntitySync(passport_guid);
  ASSERT_TRUE(cached_unmasked.has_value());
  EXPECT_FALSE(cached_unmasked->IsMaskedEntity());
  EXPECT_EQ(cached_unmasked->guid(), passport_guid);
}

// Tests that if local decryption fails during GetUnmaskedSpiiEntity, it
// returns nullopt.
TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       GetUnmaskedSpiiEntity_SpiiCacheEnabled_DecryptionFailure) {
  EntityInstance::EntityId passport_guid =
      PrefetchEncryptedPassportAndGetGuid("enc_bytes", "P123", "John Doe");

  // GetUnmaskedSpiiEntity's decryption call fails.
  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("enc_bytes")))
      .WillOnce(Return(std::nullopt));
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), std::nullopt);
  histogram_tester().ExpectUniqueSample(
      "Autofill.Ai.Unmask.Result.PersonalContext",
      AutofillAiUnmaskResult::kDecryptionFailed, 1);
}

TEST_F(AutofillAiPersonalContextAccessManagerImplSpiiCacheTest,
       ConvertProtoToEntityInstance_MaskSpii) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  *response.add_entities() = CreateEncryptedEntity("enc_passport");
  const DenseSet<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};

  EXPECT_CALL(mock_personal_context_service(),
              DecryptEntity(MatchEncryptedEntity("enc_passport")))
      .WillOnce(Return(CreateDecryptedPassportEntity("P12345", "Jane Doe")));
  std::vector<EntityInstance> entities;
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))))
      .WillOnce(SaveOptSpanToVector<1>(&entities));

  PrefetchContextSync(requested_types, response);
  ASSERT_EQ(entities.size(), 1u);
  EXPECT_TRUE(entities[0].IsMaskedEntity());
  EXPECT_THAT(
      entities,
      UnorderedElementsAre(AllOf(
          Property(&EntityInstance::type,
                   Property(&EntityType::name, EntityTypeName::kPassport)),
          HasAttributeWithValue(AttributeTypeName::kPassportName, u"Jane Doe"),
          HasAttributeWithValue(AttributeTypeName::kPassportNumber, u"45"))));
}

}  // namespace
}  // namespace autofill
