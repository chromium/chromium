// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl_test_api.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
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
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

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
  PersonalContextAccessManagerImplTest() = default;
  ~PersonalContextAccessManagerImplTest() override = default;

  PersonalContextAccessManagerImpl& access_manager() { return access_manager_; }

  MockPersonalContextService& mock_personal_context_service() {
    return mock_personal_context_service_;
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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockPersonalContextService mock_personal_context_service_;
  MockPersonalContextEnablementService mock_enablement_service_;
  PersonalContextAccessManagerImpl access_manager_{
      &mock_personal_context_service_, &mock_enablement_service_};
};

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
          personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, _,
          _, _))
      .WillOnce(DoAll(
          WithArg<1>([](const google::protobuf::MessageLite& request_metadata) {
            const auto& request =
                static_cast<const personal_context::proto::
                                ContextMemoryAmbientAutofillRequest&>(
                    request_metadata);
            ASSERT_EQ(request.requested_types_size(), 1);
            EXPECT_EQ(request.requested_types(0),
                      personal_context::proto::EntityType::ORDER);
          }),
          RunOnceCallback<3>(personal_context::FetchContextResult(
              base::ok(std::move(any_response))))));

  access_manager().PrefetchAmbientAutofillContext(requested_types);

  // TODO(crbug.com/516721244): Check the cache for prefetched entities.
}

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
  EXPECT_TRUE(access_manager().GetCachedEntities().empty());
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

// Tests that caching new prefetched entities resets the unmasked SPII cache
// only for the corresponding entity types, leaving other types unaffected.
TEST_F(PersonalContextAccessManagerImplTest,
       CachePrefetchedEntities_ResetsUnmaskedCacheForType) {
  EntityInstance passport_unmasked = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance passport_masked = test::MaskEntityInstance(passport_unmasked);

  EntityInstance dl_unmasked = test::GetDriversLicenseEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  EntityInstance dl_masked = test::MaskEntityInstance(dl_unmasked);

  // 1. Cache unmasked Passport and DL.
  test_api(access_manager())
      .CachePrefetchedEntities({passport_masked, dl_masked});
  test_api(access_manager()).CacheUnmaskedSpiiEntity(passport_unmasked);
  test_api(access_manager()).CacheUnmaskedSpiiEntity(dl_unmasked);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_unmasked.guid()),
            passport_unmasked);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(dl_unmasked.guid()), dl_unmasked);

  // 2. Cache prefetched Passport. This should reset the unmasked Passport
  // cache, but NOT the DL cache (since it's a different type).
  test_api(access_manager()).CachePrefetchedEntities({passport_masked});

  EXPECT_EQ(GetUnmaskedSpiiEntitySync(passport_unmasked.guid()), std::nullopt);
  EXPECT_EQ(GetUnmaskedSpiiEntitySync(dl_unmasked.guid()), dl_unmasked);
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

}  // namespace
}  // namespace autofill
