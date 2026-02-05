// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_wallet_utils.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using test::GetPassportEntityInstance;
using test::MaskEntityInstance;
using ::testing::InSequence;
using ::testing::NiceMock;

class MockAutofillClient : public TestAutofillClient {
 public:
  MOCK_METHOD(void, CloseEntityImportBubble, (), (override));
  MOCK_METHOD(void, ShowAutofillAiLocalSaveNotification, (), (override));
};

class AutofillAiWalletUtilsTest : public ::testing::Test {
 protected:
  MockAutofillClient& autofill_client() { return autofill_client_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
};

// Tests that the import data bubble is closed after a successful Wallet upsert
// response.
TEST_F(AutofillAiWalletUtilsTest,
       HandleWalletUpsertResponseSuccessClosesBubble) {
  EXPECT_CALL(autofill_client(), CloseEntityImportBubble());

  EntityInstance passport = GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  HandleWalletUpsertResponse(
      /*entity_manager=*/nullptr, autofill_client().GetWeakPtr(),
      AutofillClient::AutofillAiImportPromptType::kSave,
      /*wallet_response=*/MaskEntityInstance(passport));
}

// Tests that the import data bubble is closed and a local save notification is
// shown after a failed Wallet upsert response when the prompt type is `kSave`.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletUpsertResponseFailureSave) {
  {
    InSequence s;
    EXPECT_CALL(autofill_client(), CloseEntityImportBubble());
    EXPECT_CALL(autofill_client(), ShowAutofillAiLocalSaveNotification());
  }

  HandleWalletUpsertResponse(
      /*entity_manager=*/nullptr, autofill_client().GetWeakPtr(),
      AutofillClient::AutofillAiImportPromptType::kSave,
      /*wallet_response=*/std::nullopt);
}

// Tests that the import data bubble is closed after a failed Wallet upsert
// response when the prompt type is `kUpdate`.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletUpsertResponseFailurepdate) {
  EXPECT_CALL(autofill_client(), CloseEntityImportBubble());
  EXPECT_CALL(autofill_client(), ShowAutofillAiLocalSaveNotification())
      .Times(0);

  HandleWalletUpsertResponse(
      /*entity_manager=*/nullptr, autofill_client().GetWeakPtr(),
      AutofillClient::AutofillAiImportPromptType::kUpdate,
      /*wallet_response=*/std::nullopt);
}

// Tests that the import data bubble is closed after a failed Wallet upsert
// response when the prompt type is `kMigrate`.
TEST_F(AutofillAiWalletUtilsTest, HandleWalletUpsertResponseFailureMigrate) {
  EXPECT_CALL(autofill_client(), CloseEntityImportBubble());
  EXPECT_CALL(autofill_client(), ShowAutofillAiLocalSaveNotification())
      .Times(0);

  HandleWalletUpsertResponse(
      /*entity_manager=*/nullptr, autofill_client().GetWeakPtr(),
      AutofillClient::AutofillAiImportPromptType::kMigrate,
      /*wallet_response=*/std::nullopt);
}

}  // namespace
}  // namespace autofill
