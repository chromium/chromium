// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"

#include <memory>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using GetUnmaskedPassCallback =
    ::wallet::WalletHttpClient::GetUnmaskedPassCallback;
using ::wallet::PrivatePass;
using WalletRequestError = ::wallet::WalletHttpClient::WalletRequestError;

constexpr std::string_view kUnmaskedValue = "unmasked";

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

EntityInstance GetServerEntityInstance(EntityTypeName entity_type) {
  switch (entity_type) {
    case EntityTypeName::kPassport:
      return test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet});
    case EntityTypeName::kDriversLicense:
      return test::GetDriversLicenseEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet});
    case EntityTypeName::kNationalIdCard:
      return test::GetNationalIdCardEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet});
    case EntityTypeName::kKnownTravelerNumber:
      return test::GetKnownTravelerNumberInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet});
    case EntityTypeName::kRedressNumber:
      return test::GetRedressNumberEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet});
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

 private:
  base::test::TaskEnvironment task_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  TestAutofillClient client_;
  EntityDataManager data_manager_;
  std::unique_ptr<WalletPassAccessManagerImpl> access_manager_;
  raw_ptr<MockWalletHttpClient> mock_http_client_;  // Owned by access_manager_
};

// Tests the happy path where unmasking succeeds.
TEST_P(WalletPassAccessManagerImplTest, GetUnmaskedWalletEntityInstance) {
  EntityInstance masked_entity =
      test::MaskEntityInstance(GetServerEntityInstance(GetParam()));
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  PrivatePass unmasked_pass;
  AttributeTypeName expected_unmasked_type;
  switch (GetParam()) {
    case EntityTypeName::kPassport:
      *unmasked_pass.mutable_passport()->mutable_passport_number() =
          kUnmaskedValue;
      expected_unmasked_type = AttributeTypeName::kPassportNumber;
      break;
    case EntityTypeName::kDriversLicense:
      *unmasked_pass.mutable_driver_license()->mutable_driver_license_number() =
          kUnmaskedValue;
      expected_unmasked_type = AttributeTypeName::kDriversLicenseNumber;
      break;
    case EntityTypeName::kNationalIdCard:
      *unmasked_pass.mutable_id_card()->mutable_id_number() = kUnmaskedValue;
      expected_unmasked_type = AttributeTypeName::kNationalIdCardNumber;
      break;
    case EntityTypeName::kKnownTravelerNumber:
      *unmasked_pass.mutable_known_traveler_number()
           ->mutable_known_traveler_number() = kUnmaskedValue;
      expected_unmasked_type = AttributeTypeName::kKnownTravelerNumberNumber;
      break;
    case EntityTypeName::kRedressNumber:
      *unmasked_pass.mutable_redress_number()->mutable_redress_number() =
          kUnmaskedValue;
      expected_unmasked_type = AttributeTypeName::kRedressNumberNumber;
      break;
    default:
      NOTREACHED();
  }
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(std::move(unmasked_pass)));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result;
  access_manager().GetUnmaskedWalletEntityInstance(masked_entity.guid(),
                                                   unmask_result.GetCallback());

  AttributeInstance expected_unmasked_attribute(
      (AttributeType(expected_unmasked_type)));
  expected_unmasked_attribute.SetRawInfo(
      expected_unmasked_attribute.type().field_type(),
      base::UTF8ToUTF16(kUnmaskedValue), VerificationStatus::kNoStatus);
  EXPECT_EQ(unmask_result.Get(), masked_entity.CopyWithUpdatedAttribute(
                                     expected_unmasked_attribute));
}

// Tests that unmasking fails if no entity if found in the data manager.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_NoEntity) {
  EntityInstance masked_entity =
      test::MaskEntityInstance(GetServerEntityInstance(GetParam()));
  EXPECT_CALL(mock_http_client(), GetUnmaskedPass).Times(0);
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result;
  access_manager().GetUnmaskedWalletEntityInstance(masked_entity.guid(),
                                                   unmask_result.GetCallback());
  EXPECT_FALSE(unmask_result.Get().has_value());
}

// Tests that unmasking fails when Wallet returns an error.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_ErrorResponse) {
  EntityInstance masked_entity =
      test::MaskEntityInstance(GetServerEntityInstance(GetParam()));
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

// Tests that unmasking fails when Wallet returns a malformed response with a
// different pass number than the entity that was requested.
TEST_P(WalletPassAccessManagerImplTest,
       GetUnmaskedWalletEntityInstance_MalformedResponse) {
  EntityInstance masked_entity =
      test::MaskEntityInstance(GetServerEntityInstance(GetParam()));
  data_manager().AddOrUpdateEntityInstance(masked_entity);
  webdata_helper().WaitUntilIdle();

  PrivatePass malformed_pass;
  // Return a response for a different entity type.
  if (masked_entity.type().name() == EntityTypeName::kPassport) {
    *malformed_pass.mutable_id_card()->mutable_id_number() = kUnmaskedValue;
  } else {
    *malformed_pass.mutable_passport()->mutable_passport_number() =
        kUnmaskedValue;
  }
  EXPECT_CALL(mock_http_client(),
              GetUnmaskedPass(masked_entity.guid().value(), _))
      .WillOnce(RunOnceCallback<1>(malformed_pass));
  base::test::TestFuture<std::optional<EntityInstance>> unmask_result;
  access_manager().GetUnmaskedWalletEntityInstance(masked_entity.guid(),
                                                   unmask_result.GetCallback());
  EXPECT_FALSE(unmask_result.Get().has_value());
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
