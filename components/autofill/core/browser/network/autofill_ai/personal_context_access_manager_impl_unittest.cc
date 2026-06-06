// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl_test_api.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

[[nodiscard]] auto HasAttributeWithValue(AttributeTypeName attribute_type_name,
                                         std::u16string value) {
  return Truly([=](const EntityInstance& entity) {
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(AttributeType(attribute_type_name));
    return attribute && attribute->GetCompleteInfo(/*app_locale=*/"") == value;
  });
}

[[nodiscard]] auto AmbientAutofillFetchRequestWithType(
    std::vector<personal_context::proto::EntityType> types) {
  return ResultOf(
      [](const google::protobuf::MessageLite& request) {
        return static_cast<const personal_context::proto::
                               ContextMemoryAmbientAutofillRequest&>(request)
            .requested_types();
      },
      ElementsAreArray(types));
}

class MockPersonalContextService
    : public personal_context::PersonalContextService {
 public:
  MockPersonalContextService() = default;
  ~MockPersonalContextService() override = default;

  MOCK_METHOD(void,
              FetchContext,
              (personal_context::proto::ContextMemoryFeature feature,
               const google::protobuf::MessageLite& request_metadata,
               const personal_context::ContextMemoryRequestOptions& options,
               personal_context::FetchContextCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchPiiEntities,
              (const personal_context::proto::FetchPiiEntitiesRequest& request,
               const personal_context::ContextMemoryRequestOptions& options,
               personal_context::FetchPiiContextCallback callback),
              (override));
};

class MockPersonalContextEnablementService
    : public personal_context::PersonalContextEnablementService {
 public:
  MockPersonalContextEnablementService() = default;
  ~MockPersonalContextEnablementService() override = default;

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(personal_context::PersonalContextEnablementState,
              GetEnablementState,
              (),
              (override));
};

class PersonalContextAccessManagerImplTest : public testing::Test {
 public:
  PersonalContextAccessManagerImplTest() {
    ON_CALL(mock_enablement_service_, GetEnablementState)
        .WillByDefault(testing::Return(
            personal_context::PersonalContextEnablementState::kEnabled));
  }
  ~PersonalContextAccessManagerImplTest() override = default;

  PersonalContextAccessManagerImpl& access_manager() { return access_manager_; }

  MockPersonalContextService& mock_personal_context_service() {
    return mock_personal_context_service_;
  }

  MockPersonalContextEnablementService& mock_enablement_service() {
    return mock_enablement_service_;
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

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillAmbientAutofill};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockPersonalContextService mock_personal_context_service_;
  MockPersonalContextEnablementService mock_enablement_service_;
  PersonalContextAccessManagerImpl access_manager_{
      &mock_personal_context_service_, &mock_enablement_service_};
};

// Tests that PrefetchAmbientAutofillContext successfully requests context from
// the backend, parses the returned entities, and caches them with their TTL.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextSuccess) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_order()->set_order_id("12345");
  entity->mutable_order()->set_merchant_name("Amazon");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          AmbientAutofillFetchRequestWithType(
              {personal_context::proto::EntityType::ORDER}),
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kOrder));
  EXPECT_THAT(access_manager().GetCachedEntities(),
              UnorderedElementsAre(AllOf(
                  Property(&EntityInstance::type,
                           Property(&EntityType::name, EntityTypeName::kOrder)),
                  HasAttributeWithValue(AttributeTypeName::kOrderId, u"12345"),
                  HasAttributeWithValue(AttributeTypeName::kOrderMerchantName,
                                        u"Amazon"))));

  std::vector<EntityInstance> cached = access_manager().GetCachedEntities();
  ASSERT_EQ(cached.size(), 1u);
  EXPECT_EQ(cached[0].type().name(), EntityTypeName::kOrder);

  base::optional_ref<const AttributeInstance> order_id_attr =
      cached[0].attribute(AttributeType(AttributeTypeName::kOrderId));
  ASSERT_TRUE(order_id_attr.has_value());
  EXPECT_EQ(order_id_attr->GetCompleteRawInfo(), u"12345");

  base::optional_ref<const AttributeInstance> merchant_attr =
      cached[0].attribute(AttributeType(AttributeTypeName::kOrderMerchantName));
  ASSERT_TRUE(merchant_attr.has_value());
  EXPECT_EQ(merchant_attr->GetCompleteRawInfo(), u"Amazon");
}

// Tests that PrefetchAmbientAutofillContext filters out and only requests
// entity types that are not currently cached.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextOnlyRequestsUncachedTypes) {
  // 1. First, cache Passport.
  EntityInstance passport =
      test::MaskEntityInstance(test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kPersonalContext}));
  test_api(access_manager()).CachePrefetchedEntities({passport});
  ASSERT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));

  // 2. Now call PrefetchAmbientAutofillContext for both Passport and Driver's
  // License. It should only request Driver's License.
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport),
      EntityType(EntityTypeName::kDriversLicense)};

  personal_context::proto::ContextMemoryAmbientAutofillResponse
      expected_response;
  personal_context::proto::Entity* entity = expected_response.add_entities();
  entity->mutable_drivers_license()->set_number("DL98765");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL,
          // Only DRIVERS_LICENSE should be in the request, not PASSPORT.
          AmbientAutofillFetchRequestWithType(
              {personal_context::proto::EntityType::DRIVERS_LICENSE}),
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  // Both should now be cached.
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kDriversLicense));
}

// Tests that PrefetchAmbientAutofillContext immediately returns and triggers
// no network requests when all requested entity types are already cached.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextAllCachedNoRequest) {
  // 1. Cache Passport.
  EntityInstance passport =
      test::MaskEntityInstance(test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kPersonalContext}));
  test_api(access_manager()).CachePrefetchedEntities({passport});
  ASSERT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));

  // 2. Call PrefetchAmbientAutofillContext for Passport.
  // No network request should be made.
  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kPassport)};
  access_manager().PrefetchAmbientAutofillContext(requested_types);
}

// Tests that PrefetchAmbientAutofillContext does not cache anything or
// mark types as cached when the fetch context request fails.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextFailure) {
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

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kOrder));
  EXPECT_THAT(access_manager().GetCachedEntities(), IsEmpty());
}

// Tests that `PrefetchAmbientAutofillContext` marks requested types as cached
// even when the response is empty.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContextNegativeCaching) {
  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder),
      EntityType(EntityTypeName::kPassport)};

  // Empty response.
  personal_context::proto::ContextMemoryAmbientAutofillResponse empty_response;
  personal_context::proto::Any any_response;
  empty_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  // Both types should be marked as cached, but have no entities.
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kOrder));
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_THAT(access_manager().GetCachedEntities(), IsEmpty());
}

// Tests that prefetched entities are cached with a 30-minute TTL, and that the
// TTL is tracked per entity type.
TEST_F(PersonalContextAccessManagerImplTest, CachePrefetchedEntities_TTL) {
  EntityInstance passport =
      test::MaskEntityInstance(test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kPersonalContext}));
  EntityInstance dl =
      test::MaskEntityInstance(test::GetDriversLicenseEntityInstance(
          {.record_type = EntityInstance::RecordType::kPersonalContext}));

  // 1. Cache Passport.
  test_api(access_manager()).CachePrefetchedEntities({passport});
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kDriversLicense));
  EXPECT_EQ(access_manager().GetCachedEntity(passport.guid()), passport);

  // Fast forward 15 minutes (Passport still valid).
  FastForwardBy(base::Minutes(15));
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_EQ(access_manager().GetCachedEntity(passport.guid()), passport);

  // 2. Cache DL at T+15.
  test_api(access_manager()).CachePrefetchedEntities({dl});
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kDriversLicense));

  // Fast forward another 15 minutes (Total T+30). Passport should expire, DL
  // should be valid.
  FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kDriversLicense));
  EXPECT_EQ(access_manager().GetCachedEntity(passport.guid()), std::nullopt);
  EXPECT_EQ(access_manager().GetCachedEntity(dl.guid()), dl);

  // Fast forward another 15 minutes (Total T+45). DL should expire.
  FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kDriversLicense));
  EXPECT_EQ(access_manager().GetCachedEntity(dl.guid()), std::nullopt);
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

// Tests that resetting the cache for a type clears any existing cached entities
// of that type (useful for caching empty results).
TEST_F(PersonalContextAccessManagerImplTest, ResetCacheForType) {
  EntityInstance passport =
      test::MaskEntityInstance(test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kPersonalContext}));

  // Cache passport.
  test_api(access_manager()).CachePrefetchedEntities({passport});
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_EQ(access_manager().GetCachedEntity(passport.guid()), passport);

  // Reset cache (empty). Should clear passport.
  test_api(access_manager()).ResetCacheForType(EntityTypeName::kPassport);
  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_EQ(access_manager().GetCachedEntity(passport.guid()), std::nullopt);
}

// Tests that GetCachedEntities returns only the prefetched (masked)
// entities and excludes any unmasked SPII entities.
TEST_F(PersonalContextAccessManagerImplTest, GetCachedEntities) {
  EntityInstance passport_unmasked = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance passport_masked = test::MaskEntityInstance(passport_unmasked);

  EntityInstance dl_masked =
      test::MaskEntityInstance(test::GetDriversLicenseEntityInstance(
          {.record_type = EntityInstance::RecordType::kPersonalContext}));

  // Cache prefetched (masked Passport and DL).
  test_api(access_manager())
      .CachePrefetchedEntities({passport_masked, dl_masked});

  // GetCachedEntities should return masked Passport and DL.
  EXPECT_THAT(access_manager().GetCachedEntities(),
              UnorderedElementsAre(passport_masked, dl_masked));

  // Now cache unmasked Passport.
  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport_unmasked);

  // GetCachedEntities should STILL return masked Passport and DL.
  EXPECT_THAT(access_manager().GetCachedEntities(),
              UnorderedElementsAre(passport_masked, dl_masked));
}

// Tests that natural expiration of the prefetched cache also evicts any
// corresponding unmasked SPII entities, even if they haven't reached their
// individual TTL yet.
TEST_F(PersonalContextAccessManagerImplTest,
       CachePrefetchedEntities_ExpirationResetsUnmaskedCache) {
  EntityInstance passport_unmasked = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance passport_masked = test::MaskEntityInstance(passport_unmasked);

  // 1. Cache prefetched (masked) Passport at T=0.
  test_api(access_manager()).CachePrefetchedEntities({passport_masked});
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));

  // 2. Fast forward 29.5 minutes.
  FastForwardBy(base::Minutes(29) + base::Seconds(30));

  // The prefetched cache is still valid (expires in 30 seconds).
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_EQ(access_manager().GetCachedEntity(passport_masked.guid()),
            passport_masked);

  // 3. Cache unmasked SPII Passport at T=29.5.
  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport_unmasked);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_unmasked.guid()),
            passport_unmasked);

  // 4. Fast forward 30 seconds (Total T+30). The prefetched cache expires.
  // This should also trigger the eviction of the unmasked SPII cache.
  FastForwardBy(base::Seconds(30));

  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_EQ(access_manager().GetCachedEntity(passport_masked.guid()),
            std::nullopt);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_unmasked.guid()), std::nullopt);
}

// Tests that PrefetchAmbientAutofillContext is not executed if the
// kAutofillAmbientAutofill flag is disabled.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContext_FlagDisabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(features::kAutofillAmbientAutofill);

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kOrder));
}

// Tests that PrefetchAmbientAutofillContext is not executed if the
// enablement state does not return an enabled state.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContext_EnablementDisabled) {
  EXPECT_CALL(mock_enablement_service(), GetEnablementState)
      .WillRepeatedly(
          testing::Return(personal_context::PersonalContextEnablementState::
                              kDisabledNotEligible));

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  EXPECT_CALL(mock_personal_context_service(), FetchContext).Times(0);
  access_manager().PrefetchAmbientAutofillContext(requested_types);
  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kOrder));
}

// Tests that PrefetchAmbientAutofillContext is executed if the
// enablement state is kEnabledShouldShowNotice.
TEST_F(PersonalContextAccessManagerImplTest,
       PrefetchAmbientAutofillContext_EnabledShouldShowNotice) {
  EXPECT_CALL(mock_enablement_service(), GetEnablementState)
      .WillRepeatedly(
          testing::Return(personal_context::PersonalContextEnablementState::
                              kEnabledShouldShowNotice));

  const std::vector<EntityType> requested_types = {
      EntityType(EntityTypeName::kOrder)};

  auto create_expected_response = []() -> personal_context::proto::Any {
    personal_context::proto::ContextMemoryAmbientAutofillResponse
        expected_response;
    personal_context::proto::Entity* entity = expected_response.add_entities();
    entity->mutable_order()->set_order_id("12345");
    entity->mutable_order()->set_merchant_name("Amazon");

    personal_context::proto::Any any_response;
    expected_response.SerializeToString(any_response.mutable_value());
    return any_response;
  };

  EXPECT_CALL(
      mock_personal_context_service(),
      FetchContext(
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(create_expected_response()))));

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kOrder));
}

// Tests that when OnEnablementStateChanged is called with a disabled state, all
// caches are wiped.
TEST_F(PersonalContextAccessManagerImplTest, WipeCachesOnDisablement) {
  EntityInstance passport =
      test::MaskEntityInstance(test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kPersonalContext}));

  // 1. Cache prefetched (masked) passport.
  test_api(access_manager()).CachePrefetchedEntities({passport});
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_EQ(access_manager().GetCachedEntity(passport.guid()), passport);

  // 2. Call OnEnablementStateChanged with an ENABLED state. Caches should not
  // be wiped.
  access_manager().OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState::
          kEnabledShouldShowNotice);
  EXPECT_TRUE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_EQ(access_manager().GetCachedEntity(passport.guid()), passport);

  // 3. Call OnEnablementStateChanged with a DISABLED state. Caches should be
  // wiped.
  access_manager().OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState::
          kDisabledViaPersonalIntelligenceInAutofillToggle);
  EXPECT_FALSE(access_manager().IsTypeCached(EntityTypeName::kPassport));
  EXPECT_EQ(access_manager().GetCachedEntity(passport.guid()), std::nullopt);
}

}  // namespace
}  // namespace autofill
