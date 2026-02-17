// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"

#include <memory>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::Truly;
using GetUnmaskedPassCallback =
    ::wallet::WalletHttpClient::GetUnmaskedPassCallback;
using ::wallet::PrivatePass;
using WalletRequestError = ::wallet::WalletHttpClient::WalletRequestError;

constexpr std::string_view kMaskedValue = "5678";
constexpr std::string_view kUnmaskedValue = "12345678";

class MockWalletHttpClient : public wallet::WalletHttpClient {
 public:
  MOCK_METHOD(void,
              UpsertPublicPass,
              (wallet::Pass pass, UpsertPublicPassCallback callback),
              (override));
  MOCK_METHOD(void,
              UpsertPrivatePass,
              (PrivatePass pass, UpsertPrivatePassCallback callback),
              (override));
  MOCK_METHOD(void,
              GetUnmaskedPass,
              (std::string_view pass_id, GetUnmaskedPassCallback callback),
              (override));
};

EntityInstance GetUnmaskedServerEntityInstance(
    EntityTypeName entity_type,
    std::string_view unmasked_number) {
  std::u16string u16number = base::UTF8ToUTF16(unmasked_number);
  switch (entity_type) {
    case EntityTypeName::kPassport:
      return test::GetPassportEntityInstance(
          {.number = u16number.c_str(),
           .record_type = EntityInstance::RecordType::kServerWallet});
    case EntityTypeName::kDriversLicense:
      return test::GetDriversLicenseEntityInstance(
          {.number = u16number.c_str(),
           .record_type = EntityInstance::RecordType::kServerWallet});
    case EntityTypeName::kNationalIdCard:
      return test::GetNationalIdCardEntityInstance(
          {.number = u16number.c_str(),
           .record_type = EntityInstance::RecordType::kServerWallet});
    case EntityTypeName::kKnownTravelerNumber:
      return test::GetKnownTravelerNumberInstance(
          {.number = u16number.c_str(),
           .record_type = EntityInstance::RecordType::kServerWallet});
    case EntityTypeName::kRedressNumber:
      return test::GetRedressNumberEntityInstance(
          {.number = u16number.c_str(),
           .record_type = EntityInstance::RecordType::kServerWallet});
    default:
      NOTREACHED();
  }
}

PrivatePass CreatePassWithNumber(EntityTypeName pass_type,
                                 std::string_view number) {
  PrivatePass pass;
  switch (pass_type) {
    case EntityTypeName::kPassport:
      pass.mutable_passport()->set_passport_number(number);
      break;
    case EntityTypeName::kDriversLicense:
      pass.mutable_driver_license()->set_driver_license_number(number);
      break;
    case EntityTypeName::kNationalIdCard:
      pass.mutable_id_card()->set_id_number(number);
      break;
    case EntityTypeName::kKnownTravelerNumber:
      pass.mutable_known_traveler_number()->set_known_traveler_number(number);
      break;
    case EntityTypeName::kRedressNumber:
      pass.mutable_redress_number()->set_redress_number(number);
      break;
    default:
      NOTREACHED();
  }
  return pass;
}

AttributeType GetPassNumberAttribute(EntityTypeName entity_type) {
  switch (entity_type) {
    case EntityTypeName::kPassport:
      return AttributeType(AttributeTypeName::kPassportNumber);
    case EntityTypeName::kDriversLicense:
      return AttributeType(AttributeTypeName::kDriversLicenseNumber);
    case EntityTypeName::kNationalIdCard:
      return AttributeType(AttributeTypeName::kNationalIdCardNumber);
    case EntityTypeName::kKnownTravelerNumber:
      return AttributeType(AttributeTypeName::kKnownTravelerNumberNumber);
    case EntityTypeName::kRedressNumber:
      return AttributeType(AttributeTypeName::kRedressNumberNumber);
    default:
      NOTREACHED();
  }
}

class WalletPassAccessManagerImplTest
    : public testing::TestWithParam<EntityTypeName> {
 public:
  WalletPassAccessManagerImplTest()
      : data_manager_(client_.GetPrefs(),
                      client_.GetIdentityManager(),
                      client_.GetSyncService(),
                      webdata_helper_.autofill_webdata_service(),
                      /*history_service=*/nullptr,
                      /*strike_database=*/nullptr,
                      /*accessibility_annotator_data_adapter=*/nullptr,
                      /*variation_country_code=*/GeoIpCountryCode("US")) {
    client_.SetUpPrefsAndIdentityForAutofillAi();
    auto http_client = std::make_unique<MockWalletHttpClient>();
    mock_http_client_ = http_client.get();
    access_manager_ = std::make_unique<WalletPassAccessManagerImpl>(
        std::move(http_client), &data_manager_);
  }

  WalletPassAccessManagerImpl& access_manager() { return *access_manager_; }
  MockWalletHttpClient& mock_http_client() { return *mock_http_client_; }
  EntityDataManager& data_manager() { return data_manager_; }
  AutofillWebDataServiceTestHelper& webdata_helper() { return webdata_helper_; }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_{
      features::kAutofillAiWalletPrivatePasses};
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  TestAutofillClient client_;
  EntityDataManager data_manager_;
  std::unique_ptr<WalletPassAccessManagerImpl> access_manager_;
  raw_ptr<MockWalletHttpClient> mock_http_client_;  // Owned by access_manager_
};

// Tests the happy path where unmasking succeeds.
TEST_P(WalletPassAccessManagerImplTest, GetUnmaskedWalletEntityInstance) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);
  EntityInstance masked_entity = test::MaskEntityInstance(unmasked_entity);
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  PrivatePass unmasked_pass = CreatePassWithNumber(GetParam(), kUnmaskedValue);
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(std::move(unmasked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result;
  access_manager().GetUnmaskedWalletEntityInstance(masked_entity.guid(),
                                                   unmask_result.GetCallback());
  EXPECT_EQ(unmask_result.Get(), unmasked_entity);
}

// Tests that unmasking results are cached.
TEST_P(WalletPassAccessManagerImplTest, GetUnmaskedWalletEntityInstance_Cache) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);
  EntityInstance masked_entity = test::MaskEntityInstance(unmasked_entity);
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  // Initial unmasking call: Expect the http client to be called.
  PrivatePass unmasked_pass = CreatePassWithNumber(GetParam(), kUnmaskedValue);
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(std::move(unmasked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result1;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result1.GetCallback());
  EXPECT_EQ(unmask_result1.Get(), unmasked_entity);

  // Second unmasking call. Expect no more network calls.
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result2;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result2.GetCallback());
  EXPECT_EQ(unmask_result2.Get(), unmasked_entity);
}

// Tests that the `WalletPassAccessManagerImpl::kCacheTTL` is respected.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_CacheTTL) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);
  EntityInstance masked_entity = test::MaskEntityInstance(unmasked_entity);
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  // Expect two calls to the http client, since the cache is expected to expire
  // after kCacheTTL.
  PrivatePass unmasked_pass = CreatePassWithNumber(GetParam(), kUnmaskedValue);
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(std::move(unmasked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result1;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result1.GetCallback());
  EXPECT_EQ(unmask_result1.Get(), unmasked_entity);

  // Wait for kCacheTTL and unmask again.
  FastForwardBy(WalletPassAccessManagerImpl::kCacheTTL);
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result2;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result2.GetCallback());
  EXPECT_EQ(unmask_result2.Get(), unmasked_entity);
}

// Tests that when an entity is removed from the data manager, its cache entry
// is removed.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_CacheInvalidateOnRemove) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);
  EntityInstance masked_entity = test::MaskEntityInstance(unmasked_entity);
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  // Expect one call to the http client, since the second unmasking call is
  // expected to fail after the entity was removed.
  PrivatePass unmasked_pass = CreatePassWithNumber(GetParam(), kUnmaskedValue);
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(std::move(unmasked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result1;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result1.GetCallback());
  EXPECT_EQ(unmask_result1.Get(), unmasked_entity);

  // Remove the `masked_entity` in the data manager and unmask again.
  data_manager().RemoveEntityInstance(masked_entity.guid());
  webdata_helper().WaitUntilIdle();
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result2;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result2.GetCallback());
  EXPECT_FALSE(unmask_result2.Get().has_value());
}

// Tests that when an entity is updated in the data manager, its cache entry
// is removed.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_CacheInvalidateOnUpdate) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), "12345678");
  EntityInstance masked_entity = test::MaskEntityInstance(unmasked_entity);
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();
  EntityInstance updated_unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), "23456789")
          .CopyWithNewEntityId(masked_entity.guid());
  EntityInstance updated_masked_entity =
      test::MaskEntityInstance(updated_unmasked_entity);

  // Expect two call to the http client, since updating the entity invalidates
  // the cache entry.
  PrivatePass unmasked_pass1 = CreatePassWithNumber(GetParam(), "12345678");
  PrivatePass unmasked_pass2 = CreatePassWithNumber(GetParam(), "23456789");
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallbackRepeatedly<1>(std::move(unmasked_pass1)))
      .WillOnce(RunOnceCallbackRepeatedly<1>(std::move(unmasked_pass2)));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result1;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result1.GetCallback());
  EXPECT_EQ(unmask_result1.Get(), unmasked_entity);

  // Update the `masked_entity` in the data manager and unmask again.
  data_manager().AddOrUpdateEntityInstance(updated_masked_entity);
  webdata_helper().WaitUntilIdle();
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result2;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result2.GetCallback());
  EXPECT_EQ(unmask_result2.Get(), updated_unmasked_entity);
}

// Tests that when the access manager is notified of an unrelated data manager
// change, cache entries are kept.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_CacheDoesntInvalidateOnUnrelatedUpdate) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);
  EntityInstance masked_entity = test::MaskEntityInstance(unmasked_entity);
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  // Expect one call to the http client, since the cache is not supposed to get
  // invalidated.
  PrivatePass unmasked_pass = CreatePassWithNumber(GetParam(), kUnmaskedValue);
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(std::move(unmasked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result1;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result1.GetCallback());
  EXPECT_EQ(unmask_result1.Get(), unmasked_entity);

  // Trigger an unrelated data manager change and unmask again.
  access_manager().OnEntityInstancesChanged();
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result2;
  access_manager().GetUnmaskedWalletEntityInstance(
      masked_entity.guid(), unmask_result2.GetCallback());
  EXPECT_EQ(unmask_result2.Get(), unmasked_entity);
}

// Tests that unmasking fails if no entity if found in the data manager.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_NoEntity) {
  EntityInstance masked_entity = test::MaskEntityInstance(
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue));
  EXPECT_CALL(mock_http_client(), GetUnmaskedPass).Times(0);
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result;
  access_manager().GetUnmaskedWalletEntityInstance(masked_entity.guid(),
                                                   unmask_result.GetCallback());
  EXPECT_FALSE(unmask_result.Get().has_value());
}

// Tests that unmasking fails when Wallet returns an error.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_ErrorResponse) {
  EntityInstance masked_entity = test::MaskEntityInstance(
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue));
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(WalletRequestError::kGenericError)));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result;
  access_manager().GetUnmaskedWalletEntityInstance(masked_entity.guid(),
                                                   unmask_result.GetCallback());
  EXPECT_FALSE(unmask_result.Get().has_value());
}

// Tests that unmasking fails when Wallet returns a malformed response without a
// pass number.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_MalformedResponseNoNumber) {
  EntityInstance masked_entity = test::MaskEntityInstance(
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue));
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(PrivatePass()));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result;
  access_manager().GetUnmaskedWalletEntityInstance(masked_entity.guid(),
                                                   unmask_result.GetCallback());
  EXPECT_FALSE(unmask_result.Get().has_value());
}

// Tests that unmasking fails when Wallet returns a malformed response with a
// different pass number than the entity that was requested.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_MalformedResponseDifferentNumber) {
  EntityInstance masked_entity = test::MaskEntityInstance(
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue));
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  PrivatePass malformed_pass;
  // Return a response for a different entity type.
  if (masked_entity.type().name() == EntityTypeName::kPassport) {
    malformed_pass.mutable_id_card()->set_id_number(kUnmaskedValue);
  } else {
    malformed_pass.mutable_passport()->set_passport_number(kUnmaskedValue);
  }
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(malformed_pass));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result;
  access_manager().GetUnmaskedWalletEntityInstance(masked_entity.guid(),
                                                   unmask_result.GetCallback());
  EXPECT_FALSE(unmask_result.Get().has_value());
}

// Tests that when saving a new pass:
// - No ID is provided to the Upsert call.
// - A masked pass with the server-provided ID is returned to the caller.
TEST_P(WalletPassAccessManagerImplTest, SaveWalletEntityInstance) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);

  PrivatePass masked_pass = CreatePassWithNumber(GetParam(), kMaskedValue);
  masked_pass.set_pass_id("updated-id");
  EXPECT_CALL(mock_http_client(),
              UpsertPrivatePass(Truly([](const PrivatePass& pass) {
                                  return !pass.has_pass_id();
                                }),
                                _))
      .WillOnce(RunOnceCallback<1>(std::move(masked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> save_result;
  access_manager().SaveWalletEntityInstance(unmasked_entity,
                                            save_result.GetCallback());

  AttributeInstance expected_masked_attribute(
      GetPassNumberAttribute(GetParam()));
  expected_masked_attribute.SetRawInfo(
      expected_masked_attribute.type().field_type(),
      base::UTF8ToUTF16(kMaskedValue), VerificationStatus::kNoStatus);
  test_api(expected_masked_attribute).mark_as_masked();
  EXPECT_EQ(save_result.Get(),
            unmasked_entity
                .CopyWithNewEntityId(EntityInstance::EntityId("updated-id"))
                .CopyWithUpdatedAttribute(expected_masked_attribute));
}

// Tests that when updating an existing pass:
// - The pass ID is provided to the Upsert call.
// - A masked pass with the server-provided ID is returned to the caller.
TEST_P(WalletPassAccessManagerImplTest, UpdateWalletEntityInstance) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);

  PrivatePass masked_pass = CreatePassWithNumber(GetParam(), kMaskedValue);
  masked_pass.set_pass_id("updated-id");
  EXPECT_CALL(mock_http_client(),
              UpsertPrivatePass(Truly([&](const PrivatePass& pass) {
                                  return pass.pass_id() ==
                                         unmasked_entity.guid().value();
                                }),
                                _))
      .WillOnce(RunOnceCallback<1>(std::move(masked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> update_result;
  access_manager().UpdateWalletEntityInstance(unmasked_entity,
                                              update_result.GetCallback());

  AttributeInstance expected_masked_attribute(
      GetPassNumberAttribute(GetParam()));
  expected_masked_attribute.SetRawInfo(
      expected_masked_attribute.type().field_type(),
      base::UTF8ToUTF16(kMaskedValue), VerificationStatus::kNoStatus);
  test_api(expected_masked_attribute).mark_as_masked();
  EXPECT_EQ(update_result.Get(),
            unmasked_entity
                .CopyWithNewEntityId(EntityInstance::EntityId("updated-id"))
                .CopyWithUpdatedAttribute(expected_masked_attribute));
}

// Tests that saving and updating fails when Wallet returns an error upserting.
TEST_P(WalletPassAccessManagerImplTest,
       UpsertWalletEntityInstance_ErrorResponse) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);
  EXPECT_CALL(mock_http_client(), UpsertPrivatePass)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(
          base::unexpected(WalletRequestError::kGenericError)));

  base::test::TestFuture<std::optional<EntityInstance>> save_result;
  access_manager().SaveWalletEntityInstance(unmasked_entity,
                                            save_result.GetCallback());
  EXPECT_FALSE(save_result.Get().has_value());

  base::test::TestFuture<std::optional<EntityInstance>> update_result;
  access_manager().UpdateWalletEntityInstance(unmasked_entity,
                                              update_result.GetCallback());
  EXPECT_FALSE(update_result.Get().has_value());
}

// Tests that saving and updating fails when Wallet returns a malformed response
// without a pass number.
TEST_P(WalletPassAccessManagerImplTest,
       UpsertWalletEntityInstance_MalformedResponseNoNumber) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);
  EXPECT_CALL(mock_http_client(), UpsertPrivatePass)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(PrivatePass()));

  base::test::TestFuture<std::optional<EntityInstance>> save_result;
  access_manager().SaveWalletEntityInstance(unmasked_entity,
                                            save_result.GetCallback());
  EXPECT_FALSE(save_result.Get().has_value());

  base::test::TestFuture<std::optional<EntityInstance>> update_result;
  access_manager().UpdateWalletEntityInstance(unmasked_entity,
                                              update_result.GetCallback());
  EXPECT_FALSE(update_result.Get().has_value());
}

// Tests that saving and updating fails when Wallet returns a malformed response
// with a different pass number than the entity that was upserted.
TEST_P(WalletPassAccessManagerImplTest,
       UpsertWalletEntityInstance_MalformedResponseDifferentNumber) {
  EntityInstance unmasked_entity =
      GetUnmaskedServerEntityInstance(GetParam(), kUnmaskedValue);

  PrivatePass malformed_pass;
  // Return a response for a different entity type.
  if (unmasked_entity.type().name() == EntityTypeName::kPassport) {
    malformed_pass.mutable_id_card()->set_id_number(kMaskedValue);
  } else {
    malformed_pass.mutable_passport()->set_passport_number(kMaskedValue);
  }
  EXPECT_CALL(mock_http_client(), UpsertPrivatePass)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(malformed_pass));

  base::test::TestFuture<std::optional<EntityInstance>> save_result;
  access_manager().SaveWalletEntityInstance(unmasked_entity,
                                            save_result.GetCallback());
  EXPECT_FALSE(save_result.Get().has_value());

  base::test::TestFuture<std::optional<EntityInstance>> update_result;
  access_manager().UpdateWalletEntityInstance(unmasked_entity,
                                              update_result.GetCallback());
  EXPECT_FALSE(update_result.Get().has_value());
}

INSTANTIATE_TEST_SUITE_P(,
                         WalletPassAccessManagerImplTest,
                         testing::Values(EntityTypeName::kPassport,
                                         EntityTypeName::kDriversLicense,
                                         EntityTypeName::kNationalIdCard,
                                         EntityTypeName::kRedressNumber,
                                         EntityTypeName::kNationalIdCard));

}  // namespace

}  // namespace autofill
