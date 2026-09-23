// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"

#include <memory>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/proto/common.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::ErrorIs;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::Field;
using ::testing::Truly;
using GetUnmaskedPassCallback =
    ::wallet::WalletHttpClient::GetUnmaskedPassCallback;
using ::wallet::LegalMessage;
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
              (PrivatePass pass,
               std::optional<consent_auditor::ConsentAuditor::SessionId>,
               UpsertPrivatePassCallback callback),
              (override));
  MOCK_METHOD(void,
              GetUnmaskedPass,
              (std::string_view pass_id, GetUnmaskedPassCallback callback),
              (override));
  MOCK_METHOD(
      void,
      GetDetailsForUpsertPass,
      (wallet::WalletHttpClient::PassType pass_type,
       wallet::WalletHttpClient::GetDetailsForUpsertPassCallback callback),
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

wallet::WalletHttpClient::PassUpsertDetails CreateTestPassUpsertDetails() {
  LegalMessage legal_message;
  LegalMessage::Line* line = legal_message.add_line();
  line->set_template_("The terms are {0} and {1}.");
  LegalMessage::Link* link1 = line->add_template_parameter();
  link1->set_display_text("Terms");
  link1->set_url("https://example.com/terms");
  LegalMessage::Link* link2 = line->add_template_parameter();
  link2->set_display_text("Privacy");
  link2->set_url("https://example.com/privacy");
  legal_message.set_token("test_token");

  return wallet::WalletHttpClient::PassUpsertDetails{
      .context_token = "test_context_token",
      .legal_message = std::move(legal_message),
      .user_eligibility = wallet::WalletHttpClient::UserEligibility::kEligible,
  };
}

WalletPassAccessManager::GetDetailsForUpsertPassResponse
CreateExpectedUpsertPassResponse() {
  return WalletPassAccessManager::GetDetailsForUpsertPassResponse{
      .legal_message_lines = {TestLegalMessageLine(
          "The terms are Terms and Privacy.",
          {LegalMessageLine::Link(14, 19, "https://example.com/terms"),
           LegalMessageLine::Link(24, 31, "https://example.com/privacy")})},
      .context_token = "test_context_token",
      .user_eligibility = WalletPassAccessManager::UserEligibility::kEligible,
  };
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
                      /*pcontext_manager=*/nullptr,
                      /*strike_database=*/nullptr,
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
  consent_auditor::ConsentAuditor::SessionId session_id =
      consent_auditor::ConsentAuditor::GenerateSessionId();
  masked_pass.set_pass_id("updated-id");
  EXPECT_CALL(mock_http_client(),
              UpsertPrivatePass(Truly([](const PrivatePass& pass) {
                                  return !pass.has_pass_id();
                                }),
                                testing::Optional(session_id), _))
      .WillOnce(RunOnceCallback<2>(std::move(masked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> save_result;
  access_manager().SaveWalletEntityInstance(unmasked_entity, session_id,
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
                                testing::Eq(std::nullopt), _))
      .WillOnce(RunOnceCallback<2>(std::move(masked_pass)));
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
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          base::unexpected(WalletRequestError::kGenericError)));

  base::test::TestFuture<std::optional<EntityInstance>> save_result;
  access_manager().SaveWalletEntityInstance(
      unmasked_entity, consent_auditor::ConsentAuditor::GenerateSessionId(),
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
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(PrivatePass()));

  base::test::TestFuture<std::optional<EntityInstance>> save_result;
  access_manager().SaveWalletEntityInstance(
      unmasked_entity, consent_auditor::ConsentAuditor::GenerateSessionId(),
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
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(malformed_pass));

  base::test::TestFuture<std::optional<EntityInstance>> save_result;
  access_manager().SaveWalletEntityInstance(
      unmasked_entity, consent_auditor::ConsentAuditor::GenerateSessionId(),
      save_result.GetCallback());
  EXPECT_FALSE(save_result.Get().has_value());

  base::test::TestFuture<std::optional<EntityInstance>> update_result;
  access_manager().UpdateWalletEntityInstance(unmasked_entity,
                                              update_result.GetCallback());
  EXPECT_FALSE(update_result.Get().has_value());
}

// Tests that `GetDetailsForUpsertPass` successfully fetches legal message lines
// and context token.
TEST_P(WalletPassAccessManagerImplTest, GetDetailsForUpsertPass_Success) {
  LegalMessage legal_message;
  LegalMessage::Line* line = legal_message.add_line();
  line->set_template_("The terms are {0} and {1}.");
  LegalMessage::Link* link1 = line->add_template_parameter();
  link1->set_display_text("Terms");
  link1->set_url("https://example.com/terms");
  LegalMessage::Link* link2 = line->add_template_parameter();
  link2->set_display_text("Privacy");
  link2->set_url("https://example.com/privacy");
  legal_message.set_token("test_token");

  wallet::WalletHttpClient::PassUpsertDetails details{
      .context_token = "test_context_token",
      .legal_message = std::move(legal_message),
      .user_eligibility = wallet::WalletHttpClient::UserEligibility::kEligible,
  };

  EXPECT_CALL(mock_http_client(),
              GetDetailsForUpsertPass(
                  wallet::WalletHttpClient::PassType::kVehicleRegistration, _))
      .WillOnce(RunOnceCallback<1>(std::move(details)));

  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      future;
  access_manager().GetDetailsForUpsertPass(EntityType(EntityTypeName::kVehicle),
                                           future.GetCallback());

  const WalletPassAccessManager::GetDetailsForUpsertPassResponse
      expected_response{
          .legal_message_lines = {TestLegalMessageLine(
              "The terms are Terms and Privacy.",
              {LegalMessageLine::Link(14, 19, "https://example.com/terms"),
               LegalMessageLine::Link(24, 31, "https://example.com/privacy")})},
          .context_token = "test_context_token",
          .user_eligibility =
              WalletPassAccessManager::UserEligibility::kEligible,
      };
  EXPECT_THAT(future.Get(), ValueIs(expected_response));
}

// Tests that `GetDetailsForUpsertPass` handles responses without legal
// messages.
TEST_P(WalletPassAccessManagerImplTest,
       GetDetailsForUpsertPass_NoLegalMessage) {
  wallet::WalletHttpClient::PassUpsertDetails details{
      .context_token = "test_context_token",
      .legal_message = std::nullopt,
      .user_eligibility = wallet::WalletHttpClient::UserEligibility::kEligible,
  };

  EXPECT_CALL(mock_http_client(),
              GetDetailsForUpsertPass(
                  wallet::WalletHttpClient::PassType::kVehicleRegistration, _))
      .WillOnce(RunOnceCallback<1>(std::move(details)));

  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      future;
  access_manager().GetDetailsForUpsertPass(EntityType(EntityTypeName::kVehicle),
                                           future.GetCallback());

  const WalletPassAccessManager::GetDetailsForUpsertPassResponse
      expected_response{
          .legal_message_lines = {},
          .context_token = "test_context_token",
          .user_eligibility =
              WalletPassAccessManager::UserEligibility::kEligible,
      };
  EXPECT_THAT(future.Get(), ValueIs(expected_response));
}

// Tests that `GetDetailsForUpsertPass` handles responses without a context
// token.
TEST_P(WalletPassAccessManagerImplTest,
       GetDetailsForUpsertPass_NoContextToken) {
  wallet::WalletHttpClient::PassUpsertDetails details{
      .context_token = std::nullopt,
      .legal_message = std::nullopt,
      .user_eligibility =
          wallet::WalletHttpClient::UserEligibility::kIneligible,
  };

  EXPECT_CALL(mock_http_client(),
              GetDetailsForUpsertPass(
                  wallet::WalletHttpClient::PassType::kVehicleRegistration, _))
      .WillOnce(RunOnceCallback<1>(std::move(details)));

  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      future;
  access_manager().GetDetailsForUpsertPass(EntityType(EntityTypeName::kVehicle),
                                           future.GetCallback());

  const WalletPassAccessManager::GetDetailsForUpsertPassResponse
      expected_response{
          .legal_message_lines = {},
          .context_token = "",
          .user_eligibility =
              WalletPassAccessManager::UserEligibility::kIneligible,
      };
  EXPECT_THAT(future.Get(), ValueIs(expected_response));
}

// Tests that `GetDetailsForUpsertPass` returns error on network error.
TEST_P(WalletPassAccessManagerImplTest, GetDetailsForUpsertPass_NetworkError) {
  EXPECT_CALL(mock_http_client(),
              GetDetailsForUpsertPass(
                  wallet::WalletHttpClient::PassType::kVehicleRegistration, _))
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(WalletRequestError::kGenericError)));

  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      future;
  access_manager().GetDetailsForUpsertPass(EntityType(EntityTypeName::kVehicle),
                                           future.GetCallback());

  EXPECT_THAT(future.Get(), ErrorIs(WalletRequestError::kGenericError));
}

// Tests that calling `PreloadDetailsForUpsertPass` deduplicates requests while
// in-flight or when a valid entry is already cached.
TEST_P(WalletPassAccessManagerImplTest,
       PreloadDetailsForUpsertPass_Deduplicates) {
  wallet::WalletHttpClient::GetDetailsForUpsertPassCallback http_callback;
  EXPECT_CALL(mock_http_client(),
              GetDetailsForUpsertPass(
                  wallet::WalletHttpClient::PassType::kVehicleRegistration, _))
      .WillOnce(MoveArg<1>(&http_callback));

  // 1st preload initiates fetch.
  access_manager().PreloadDetailsForUpsertPass(
      EntityType(EntityTypeName::kVehicle));
  ASSERT_FALSE(http_callback.is_null());

  // 2nd preload while in flight is deduplicated (no duplicate network call).
  access_manager().PreloadDetailsForUpsertPass(
      EntityType(EntityTypeName::kVehicle));

  // Completing the network call populates the cache.
  std::move(http_callback).Run(CreateTestPassUpsertDetails());

  // 3rd preload while already cached is ignored.
  access_manager().PreloadDetailsForUpsertPass(
      EntityType(EntityTypeName::kVehicle));
}

// Tests that reading a preloaded response consumes and erases it from the
// cache, so that a subsequent read initiates a new network request for a fresh
// single-use token.
TEST_P(WalletPassAccessManagerImplTest,
       GetDetailsForUpsertPass_ConsumesCachedResponseOnRead) {
  EXPECT_CALL(mock_http_client(),
              GetDetailsForUpsertPass(
                  wallet::WalletHttpClient::PassType::kVehicleRegistration, _))
      .Times(2)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<1>(CreateTestPassUpsertDetails()));

  // Preload details into cache (triggers 1st network call).
  access_manager().PreloadDetailsForUpsertPass(
      EntityType(EntityTypeName::kVehicle));

  // 1st read consumes the cached response without an additional network call.
  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      future1;
  access_manager().GetDetailsForUpsertPass(EntityType(EntityTypeName::kVehicle),
                                           future1.GetCallback());
  EXPECT_THAT(future1.Get(), ValueIs(CreateExpectedUpsertPassResponse()));

  // 2nd read triggers 2nd network call because the cached response was
  // consumed on the 1st read.
  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      future2;
  access_manager().GetDetailsForUpsertPass(EntityType(EntityTypeName::kVehicle),
                                           future2.GetCallback());
  EXPECT_THAT(future2.Get(), ValueIs(CreateExpectedUpsertPassResponse()));
}

// Tests that a direct call to `GetDetailsForUpsertPass` while a background
// preload is in flight runs concurrently without blocking or coalescing, each
// receiving its own dedicated token.
TEST_P(WalletPassAccessManagerImplTest,
       GetDetailsForUpsertPass_RunsConcurrentlyWithInFlightPreload) {
  wallet::WalletHttpClient::GetDetailsForUpsertPassCallback preload_cb;
  wallet::WalletHttpClient::GetDetailsForUpsertPassCallback direct_cb;

  EXPECT_CALL(mock_http_client(),
              GetDetailsForUpsertPass(
                  wallet::WalletHttpClient::PassType::kVehicleRegistration, _))
      .Times(2)
      .WillOnce(MoveArg<1>(&preload_cb))
      .WillOnce(MoveArg<1>(&direct_cb));

  // Start background preload.
  access_manager().PreloadDetailsForUpsertPass(
      EntityType(EntityTypeName::kVehicle));
  ASSERT_FALSE(preload_cb.is_null());

  // Direct consumer calls GetDetailsForUpsertPass while preload is in flight.
  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      direct_future;
  access_manager().GetDetailsForUpsertPass(EntityType(EntityTypeName::kVehicle),
                                           direct_future.GetCallback());
  ASSERT_FALSE(direct_cb.is_null());

  // Direct fetch completes: direct caller receives unique token.
  wallet::WalletHttpClient::PassUpsertDetails direct_details =
      CreateTestPassUpsertDetails();
  direct_details.context_token = "direct_token";
  std::move(direct_cb).Run(std::move(direct_details));
  EXPECT_THAT(direct_future.Get(),
              ValueIs(Field(&WalletPassAccessManager::
                                GetDetailsForUpsertPassResponse::context_token,
                            "direct_token")));

  // Preload completes: response is cached.
  wallet::WalletHttpClient::PassUpsertDetails preload_details =
      CreateTestPassUpsertDetails();
  preload_details.context_token = "preload_token";
  std::move(preload_cb).Run(std::move(preload_details));

  // Subsequent read hits the cache populated by the preload.
  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      cached_future;
  access_manager().GetDetailsForUpsertPass(EntityType(EntityTypeName::kVehicle),
                                           cached_future.GetCallback());
  EXPECT_THAT(cached_future.Get(),
              ValueIs(Field(&WalletPassAccessManager::
                                GetDetailsForUpsertPassResponse::context_token,
                            "preload_token")));
}

#if GTEST_HAS_DEATH_TEST
// Tests that `PreloadDetailsForUpsertPass` triggers `NOTREACHED()` for
// unsupported entity types.
TEST_P(WalletPassAccessManagerImplTest,
       PreloadDetailsForUpsertPass_UnsupportedEntityType) {
  EXPECT_NOTREACHED_DEATH(
      access_manager().PreloadDetailsForUpsertPass(EntityType(GetParam())));
}

// Tests that `GetDetailsForUpsertPass` triggers `NOTREACHED()` for unsupported
// entity types.
TEST_P(WalletPassAccessManagerImplTest,
       GetDetailsForUpsertPass_UnsupportedEntityType) {
  base::test::TestFuture<
      base::expected<WalletPassAccessManager::GetDetailsForUpsertPassResponse,
                     WalletRequestError>>
      future;
  EXPECT_NOTREACHED_DEATH(access_manager().GetDetailsForUpsertPass(
      EntityType(GetParam()), future.GetCallback()));
}
#endif  // GTEST_HAS_DEATH_TEST

INSTANTIATE_TEST_SUITE_P(,
                         WalletPassAccessManagerImplTest,
                         testing::Values(EntityTypeName::kPassport,
                                         EntityTypeName::kDriversLicense,
                                         EntityTypeName::kNationalIdCard,
                                         EntityTypeName::kRedressNumber,
                                         EntityTypeName::kKnownTravelerNumber));

}  // namespace

}  // namespace autofill
