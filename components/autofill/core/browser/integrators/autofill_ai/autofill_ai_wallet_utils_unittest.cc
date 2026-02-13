// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_wallet_utils.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using test::GetNationalIdCardEntityInstance;
using test::GetPassportEntityInstance;
using test::MaskEntityInstance;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::UnorderedElementsAre;

using enum EntityInstance::RecordType;

class MockAutofillClient : public TestAutofillClient {
 public:
  MOCK_METHOD(void, CloseEntityImportBubble, (), (override));
  MOCK_METHOD(void, ShowAutofillAiLocalSaveNotification, (), (override));
  MOCK_METHOD(void,
              ShowAutofillAiFailureNotification,
              (std::u16string message),
              (override));
};

class AutofillAiWalletUtilsTest : public ::testing::Test {
 public:
  AutofillAiWalletUtilsTest() {
    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client().GetPrefs(),
            autofill_client().GetIdentityManager(),
            autofill_client().GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr,
            /*accessibility_annotator_data_adapter=*/nullptr,
            /*variation_country_code=*/GeoIpCountryCode("US")));
    // Wait until EDM has finished its load to ensure that the waits in tests
    // are not interrupted due to the notification from the initial load.
    webdata_helper_.WaitUntilIdle();
  }

 protected:
  MockAutofillClient& autofill_client() { return autofill_client_; }
  EntityDataManager& edm() { return *autofill_client().GetEntityDataManager(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  NiceMock<MockAutofillClient> autofill_client_;
};

// Tests that an invalidated pointer to `EntityDataManager` does not crash.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletUpsertResponseInvalidatedEdm) {
  EntityInstance passport =
      GetPassportEntityInstance({.record_type = kServerWallet});
  HandleWalletUpsertResponse(
      /*entity_manager=*/nullptr, autofill_client().GetWeakPtr(),
      AutofillClient::AutofillAiImportPromptType::kSave,
      /*entity=*/passport,
      /*wallet_response=*/MaskEntityInstance(passport));
}

// Tests that the import data bubble is closed after a successful Wallet save
// response and the entity is written to EDM.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletSaveResponseSuccess) {
  EXPECT_CALL(autofill_client(), CloseEntityImportBubble());

  EntityInstance passport =
      GetPassportEntityInstance({.record_type = kServerWallet});
  EntityInstance masked_passport = MaskEntityInstance(passport);
  HandleWalletUpsertResponse(edm().GetWeakPtr(), autofill_client().GetWeakPtr(),
                             AutofillClient::AutofillAiImportPromptType::kSave,
                             /*entity=*/passport,
                             /*wallet_response=*/masked_passport);
  EntityDataChangedWaiter(&edm()).Wait();
  EXPECT_THAT(edm().GetEntityInstances(),
              UnorderedElementsAre(masked_passport));
}

// Tests that the import data bubble is closed after a successful Wallet migrate
// response, that the local entity is removed and the masked server entity is
// written to EDM.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletMigrateResponseSuccess) {
  // Create pre-conditions.
  const EntityInstance local_id_card =
      GetNationalIdCardEntityInstance({.record_type = kLocal});
  edm().AddOrUpdateEntityInstance(local_id_card);
  EntityDataChangedWaiter(&edm()).Wait();
  ASSERT_THAT(edm().GetEntityInstances(), UnorderedElementsAre(local_id_card));

  // The actual behavior to test.
  EXPECT_CALL(autofill_client(), CloseEntityImportBubble());

  const EntityInstance server_id_card =
      GetNationalIdCardEntityInstance({.record_type = kServerWallet});
  const EntityInstance masked_server_id_card =
      MaskEntityInstance(server_id_card);
  HandleWalletUpsertResponse(
      edm().GetWeakPtr(), autofill_client().GetWeakPtr(),
      AutofillClient::AutofillAiImportPromptType::kMigrate,
      /*entity=*/server_id_card,
      /*wallet_response=*/masked_server_id_card);
  EntityDataChangedWaiter(&edm()).Wait(FROM_HERE, /*expected_events=*/2);
  EXPECT_THAT(edm().GetEntityInstances(),
              UnorderedElementsAre(masked_server_id_card));
}

// Tests that the import data bubble is closed after a successful Wallet update
// response and the entity is written to EDM.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletUpdateResponseSuccess) {
  // Create pre-conditions.
  const EntityInstance old_passport =
      MaskEntityInstance(GetPassportEntityInstance(
          {.name = u"Sophie", .record_type = kServerWallet}));
  const EntityInstance new_passport = GetPassportEntityInstance(
      {.name = u"Linus", .record_type = kServerWallet});
  const EntityInstance new_passport_masked = MaskEntityInstance(new_passport);
  ASSERT_NE(old_passport, new_passport_masked);
  ASSERT_EQ(old_passport.guid(), new_passport.guid());
  edm().AddOrUpdateEntityInstance(old_passport);
  EntityDataChangedWaiter(&edm()).Wait();
  ASSERT_THAT(edm().GetEntityInstances(), UnorderedElementsAre(old_passport));

  // The actual behavior to test.
  EXPECT_CALL(autofill_client(), CloseEntityImportBubble());

  HandleWalletUpsertResponse(edm().GetWeakPtr(), autofill_client().GetWeakPtr(),
                             AutofillClient::AutofillAiImportPromptType::kSave,
                             /*entity=*/new_passport,
                             /*wallet_response=*/new_passport_masked);
  EntityDataChangedWaiter(&edm()).Wait();
  EXPECT_THAT(edm().GetEntityInstances(),
              UnorderedElementsAre(new_passport_masked));
}

// Tests that the import data bubble is closed and we fall back to a local save
// if the prompt type is `kSave`.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletSaveResponseFailure) {
  {
    InSequence s;
    EXPECT_CALL(autofill_client(), CloseEntityImportBubble);
    EXPECT_CALL(autofill_client(), ShowAutofillAiLocalSaveNotification);
    EXPECT_CALL(autofill_client(), ShowAutofillAiFailureNotification).Times(0);
  }

  EntityInstance passport =
      GetPassportEntityInstance({.record_type = kServerWallet});
  HandleWalletUpsertResponse(edm().GetWeakPtr(), autofill_client().GetWeakPtr(),
                             AutofillClient::AutofillAiImportPromptType::kSave,
                             /*entity=*/passport,
                             /*wallet_response=*/std::nullopt);

  EntityDataChangedWaiter(&edm()).Wait();
  EXPECT_THAT(edm().GetEntityInstances(),
              UnorderedElementsAre(passport.CopyWithNewRecordType(kLocal)));
}

// Tests that the import data bubble is closed and an error message is shown
// after a failed Wallet upsert response when the prompt type is `kUpdate`.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletUpdateResponseFailure) {
  {
    InSequence s;
    EXPECT_CALL(autofill_client(), CloseEntityImportBubble);
    EXPECT_CALL(autofill_client(), ShowAutofillAiLocalSaveNotification)
        .Times(0);
    EXPECT_CALL(autofill_client(), ShowAutofillAiFailureNotification);
  }

  EntityInstance passport =
      GetPassportEntityInstance({.record_type = kServerWallet});
  HandleWalletUpsertResponse(
      edm().GetWeakPtr(), autofill_client().GetWeakPtr(),
      AutofillClient::AutofillAiImportPromptType::kUpdate, /*entity=*/passport,
      /*wallet_response=*/std::nullopt);
}

// Tests that the import data bubble is closed and an error message is shown
// after a failed Wallet upsert response when the prompt type is `kMigrate`.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletMigrateResponseFailure) {
  {
    InSequence s;
    EXPECT_CALL(autofill_client(), CloseEntityImportBubble());
    EXPECT_CALL(autofill_client(), ShowAutofillAiLocalSaveNotification())
        .Times(0);
    EXPECT_CALL(autofill_client(), ShowAutofillAiFailureNotification);
  }

  EntityInstance passport =
      GetPassportEntityInstance({.record_type = kServerWallet});
  HandleWalletUpsertResponse(
      edm().GetWeakPtr(), autofill_client().GetWeakPtr(),
      AutofillClient::AutofillAiImportPromptType::kMigrate, /*entity=*/passport,
      /*wallet_response=*/std::nullopt);
}

}  // namespace
}  // namespace autofill
