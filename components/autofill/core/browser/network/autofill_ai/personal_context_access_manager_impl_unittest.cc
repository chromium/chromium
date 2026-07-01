// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl_test_api.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/mock_personal_context_enablement_service.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::InvokeFuture;
using ::base::test::RunOnceCallback;
using personal_context::ContextMemoryError;
using ::personal_context::MockPersonalContextEnablementService;
using ::personal_context::MockPersonalContextService;
using ::personal_context::proto::SensitivePiiPresence;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Optional;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

using RequestStatus = PersonalContextAccessManager::RequestStatus;

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
         ExplainMatchResult(ElementsAreArray(expected_types),
                            req.requested_types(), result_listener);
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

class MockPersonalContextAccessManagerObserver
    : public PersonalContextAccessManager::Observer {
 public:
  MockPersonalContextAccessManagerObserver() = default;
  ~MockPersonalContextAccessManagerObserver() override = default;

  MOCK_METHOD(void,
              OnPrefetchContextComplete,
              (const PersonalContextAccessManager& manager,
               std::optional<base::span<const EntityInstance>> entities),
              (override));
  MOCK_METHOD(void,
              OnMaskedEntityTypeEvicted,
              (const PersonalContextAccessManager& manager, EntityType type),
              (override));
};

class PersonalContextAccessManagerImplTest : public testing::Test {
 public:
  PersonalContextAccessManagerImplTest() {
    personal_context::prefs::RegisterProfilePrefs(pref_service_.registry());
    access_manager_ = std::make_unique<PersonalContextAccessManagerImpl>(
        &mock_personal_context_service_, &mock_enablement_service_,
        &pref_service_);
    ON_CALL(mock_enablement_service_, GetEnablementState)
        .WillByDefault(testing::Return(
            personal_context::PersonalContextEnablementState::kEnabled));
    observation_.Observe(access_manager_.get());
  }
  ~PersonalContextAccessManagerImplTest() override = default;

  PersonalContextAccessManagerImpl& access_manager() {
    return *access_manager_;
  }

  MockPersonalContextService& mock_personal_context_service() {
    return mock_personal_context_service_;
  }

  MockPersonalContextEnablementService& mock_enablement_service() {
    return mock_enablement_service_;
  }

  MockPersonalContextAccessManagerObserver& mock_observer() {
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
      const std::vector<EntityType>& requested_types,
      const std::vector<EntityType>& expected_spii_types,
      const personal_context::proto::ContextMemoryAmbientAutofillResponse&
          non_spii_and_presence_response,
      const personal_context::proto::ContextMemoryAmbientAutofillResponse&
          spii_response = {}) {
    std::vector<personal_context::proto::EntityType> proto_types;
    for (const EntityType& type : requested_types) {
      if (!access_manager().IsTypePrefetched(type)) {
        proto_types.push_back(
            AutofillEntityTypeToPersonalContextEntityType(type));
      }
    }

    std::vector<personal_context::proto::EntityType> proto_spii_types;
    for (const EntityType& type : expected_spii_types) {
      proto_spii_types.push_back(
          AutofillEntityTypeToPersonalContextEntityType(type));
    }

    const bool has_spii = !expected_spii_types.empty();

    personal_context::proto::Any any_presence_response;
    non_spii_and_presence_response.SerializeToString(
        any_presence_response.mutable_value());

    personal_context::proto::Any any_spii_response;
    spii_response.SerializeToString(any_spii_response.mutable_value());

    {
      testing::InSequence s;

      EXPECT_CALL(
          mock_personal_context_service(),
          FetchContext(
              personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
              MatchContextFetchRequest(proto_types, has_spii), _, _))
          .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
              base::ok(std::move(any_presence_response)))));

      if (has_spii) {
        EXPECT_CALL(
            mock_personal_context_service(),
            FetchContext(personal_context::proto::
                             CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
                         MatchContextFetchRequest(proto_spii_types, false), _,
                         _))
            .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
                base::ok(std::move(any_spii_response)))));
      }
    }

    access_manager().PrefetchContext(requested_types);
  }

 protected:
  TestingPrefServiceSimple pref_service_;

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillAmbientAutofill};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockPersonalContextService mock_personal_context_service_;
  MockPersonalContextEnablementService mock_enablement_service_;
  std::unique_ptr<PersonalContextAccessManagerImpl> access_manager_;
  MockPersonalContextAccessManagerObserver mock_observer_;
  base::ScopedObservation<PersonalContextAccessManagerImpl,
                          MockPersonalContextAccessManagerObserver>
      observation_{&mock_observer_};
};

// Tests that PrefetchContext successfully requests context from the backend and
// parses the returned entities, notifying observers about the result.
TEST_F(PersonalContextAccessManagerImplTest, PrefetchContextSuccess) {
  const std::vector<EntityType> requested_types = {
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

// Tests that PrefetchContext filters out and only requests entity types that
// don't have a valid prefetching result available.
TEST_F(PersonalContextAccessManagerImplTest,
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
  const std::vector<EntityType> requested_types = {
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
TEST_F(PersonalContextAccessManagerImplTest,
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

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};
  access_manager().PrefetchContext(requested_types);
}

// Tests that PrefetchContext does not mark types as prefetched when the fetch
// context request fails.
TEST_F(PersonalContextAccessManagerImplTest, PrefetchContextFailure) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::ContextMemoryError expected_error =
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kGenericFailure);

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

// Tests that PrefetchContext marks requested types as prefetched even when the
// response is empty.
TEST_F(PersonalContextAccessManagerImplTest, PrefetchContextEmptyResponse) {
  const std::vector<EntityType> requested_types = {
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
TEST_F(PersonalContextAccessManagerImplTest, PrefetchedEntities_TTL) {
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
TEST_F(PersonalContextAccessManagerImplTest,
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
TEST_F(PersonalContextAccessManagerImplTest, CacheUnmaskedSpiiEntity_TTL) {
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
TEST_F(PersonalContextAccessManagerImplTest, CachePresenceSignal_TTL) {
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

// Tests that ServerHasDataAvailable returns true if presence signals are cached
// for a type.
TEST_F(PersonalContextAccessManagerImplTest, ServerHasDataAvailable) {
  const EntityType passport_type(EntityTypeName::kPassport);
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  // 1. Initially, no data is available.
  EXPECT_FALSE(access_manager().ServerHasDataAvailable(passport_type));

  // 2. Presence signal cached.
  test_api(access_manager()).CachePresenceSignal(passport_type);
  EXPECT_TRUE(access_manager().ServerHasDataAvailable(passport_type));
}

// Tests that ServerHasDataAvailable remains true even after the masked entity
// was unmasked (fetched).
TEST_F(PersonalContextAccessManagerImplTest,
       ServerHasDataAvailable_TrueAfterUnmasking) {
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
  EXPECT_TRUE(access_manager().ServerHasDataAvailable(passport_type));
}

// Tests that if the masked SPII response finishes first (which populates the
// prefetch cache and sets the status to Success), a subsequent presence signal
// response (from the first request) still correctly caches the presence signal,
// so ServerHasDataAvailable() returns true.
TEST_F(PersonalContextAccessManagerImplTest,
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
  personal_context::proto::Any any_spii_response;
  spii_response.SerializeToString(any_spii_response.mutable_value());

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      presence_response;
  presence_response.add_entities()->mutable_sensitive_pii_presence()->set_type(
      SensitivePiiPresence::PASSPORT);
  personal_context::proto::Any any_presence_response;
  presence_response.SerializeToString(any_presence_response.mutable_value());

  // 3. Complete SPII request (Request 2) first.
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(Not(IsEmpty()))));
  spii_callback_future.Take().Run(personal_context::FetchContextResult(
      base::ok(std::move(any_spii_response))));

  EXPECT_TRUE(access_manager().IsTypePrefetched(passport_type));

  // 4. Complete Presence request (Request 1) second.
  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  presence_callback_future.Take().Run(personal_context::FetchContextResult(
      base::ok(std::move(any_presence_response))));

  // `ServerHasDataAvailable` should now return true even if the presence
  // signal arrived after the SPII data was cached.
  EXPECT_TRUE(access_manager().ServerHasDataAvailable(passport_type));
}

// Tests that resetting the state for a type evicts any existing prefetched
// entities of that type.
TEST_F(PersonalContextAccessManagerImplTest, ResetStateForType) {
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
TEST_F(PersonalContextAccessManagerImplTest,
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
TEST_F(PersonalContextAccessManagerImplTest, GetUnmaskedSpiiEntity_CacheHit) {
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});

  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport);

  // No service call expected.
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities).Times(0);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport.guid()), passport);
}

// Tests that GetUnmaskedSpiiEntity triggers a service call on cache miss
// (when the masked entity is prefetched), caches the unmasked result,
// and returns it.
TEST_F(PersonalContextAccessManagerImplTest,
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
}

// Tests that `GetUnmaskedSpiiEntity` returns `std::nullopt` immediately
// if the requested entity is not prefetched.
TEST_F(PersonalContextAccessManagerImplTest,
       GetUnmaskedSpiiEntity_NotPrefetched) {
  EntityInstance::EntityId unknown_id("unknown_id");

  // No service call expected because it's not prefetched.
  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities).Times(0);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(unknown_id), std::nullopt);
}

// Tests that `GetUnmaskedSpiiEntity` returns `std::nullopt` if the service call
// fails.
TEST_F(PersonalContextAccessManagerImplTest,
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

  using personal_context::ContextMemoryError;
  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);

  EXPECT_CALL(mock_personal_context_service(), FetchPiiEntities(_, _, _))
      .WillOnce(RunOnceCallback<2>(personal_context::FetchPiiEntitiesResult(
          base::unexpected(expected_error))));

  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_guid), std::nullopt);
}

// Tests that when OnEnablementStateChanged is called with a disabled state, all
// state is wiped.
TEST_F(PersonalContextAccessManagerImplTest, WipeStateOnDisablement) {
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

  // 2. Call OnEnablementStateChanged with an ENABLED state. State should not
  // be wiped.
  access_manager().OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState::kEnabled);
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));

  // 3. Call OnEnablementStateChanged with a DISABLED state. State should be
  // wiped.
  EXPECT_CALL(mock_observer(), OnMaskedEntityTypeEvicted(
                                   _, EntityType(EntityTypeName::kPassport)));
  access_manager().OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState::kDisabledNotEligible);
  EXPECT_FALSE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kPassport)));
}

// Tests that a pending request blocks subsequent requests for the same type.
TEST_F(PersonalContextAccessManagerImplTest, PendingRequestBlocksSubsequent) {
  const std::vector<EntityType> requested_types = {
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
  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());
  future.Take().Run(
      personal_context::FetchContextResult(base::ok(std::move(any_response))));

  // Now it is prefetched.
  EXPECT_TRUE(
      access_manager().IsTypePrefetched(EntityType(EntityTypeName::kOrder)));
}

// Tests that failed requests trigger exponential backoff.
TEST_F(PersonalContextAccessManagerImplTest, FailureTriggersBackoff) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  ContextMemoryError expected_error = ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kGenericFailure);

  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

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
        .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
            base::ok(std::move(any_response)))));
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
TEST_F(PersonalContextAccessManagerImplTest, PrefetchStatusAndObserverSuccess) {
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
  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(mock_observer(),
              OnPrefetchContextComplete(_, Optional(IsEmpty())));
  future.Take().Run(
      personal_context::FetchContextResult(base::ok(std::move(any_response))));

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
TEST_F(PersonalContextAccessManagerImplTest, PrefetchStatusAndObserverFailure) {
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
  access_manager().OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState::kDisabledNotEligible);
  EXPECT_EQ(access_manager().GetPrefetchStatusByEntityType(order_type),
            RequestStatus::kNotStarted);
}

// Tests that calling PrefetchContext when all types are
// prefetched indeed notifies the observer synchronously.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchWhenAlreadyPrefetchedNotifiesObserver) {
  MockPersonalContextAccessManagerObserver observer;
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

// Tests that the state is reset when the personal context settings toggle is
// turned off.
TEST_F(PersonalContextAccessManagerImplTest,
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

}  // namespace
}  // namespace autofill
