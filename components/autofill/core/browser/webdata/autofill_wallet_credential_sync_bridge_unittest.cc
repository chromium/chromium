// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_credential_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/autofill_wallet_credential_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using sync_pb::AutofillWalletCredentialSpecifics;
using sync_pb::ModelTypeState;
using syncer::EntityData;
using syncer::MockModelTypeChangeProcessor;
using testing::NiceMock;
using testing::Return;

}  // namespace

class AutofillWalletCredentialSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    db_.AddTable(&table_);
    db_.Init(base::FilePath(WebDatabase::kInMemoryPath));
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));
    bridge_ = std::make_unique<AutofillWalletCredentialSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  EntityData SpecificsToEntity(
      const AutofillWalletCredentialSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_wallet_credential() = specifics;
    return data;
  }

  AutofillWalletCredentialSyncBridge* bridge() { return bridge_.get(); }

 private:
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<AutofillWalletCredentialSyncBridge> bridge_;
};

TEST_F(AutofillWalletCredentialSyncBridgeTest, VerifyGetClientTag) {
  std::unique_ptr<ServerCvc> server_cvc = std::make_unique<ServerCvc>(
      1234, u"890", base::Time::UnixEpoch() + base::Milliseconds(25000));

  AutofillWalletCredentialSpecifics specifics =
      AutofillWalletCredentialSpecificsFromStructData(*server_cvc);

  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            base::NumberToString(server_cvc->instrument_id));
}

TEST_F(AutofillWalletCredentialSyncBridgeTest, VerifyGetStorageKey) {
  std::unique_ptr<ServerCvc> server_cvc = std::make_unique<ServerCvc>(
      1234, u"890", base::Time::UnixEpoch() + base::Milliseconds(25000));

  AutofillWalletCredentialSpecifics specifics =
      AutofillWalletCredentialSpecificsFromStructData(*server_cvc);

  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            base::NumberToString(server_cvc->instrument_id));
}

TEST_F(AutofillWalletCredentialSyncBridgeTest, IsEntityDataValid_ValidData) {
  std::unique_ptr<ServerCvc> server_cvc = std::make_unique<ServerCvc>(
      1234, u"890", base::Time::UnixEpoch() + base::Milliseconds(25000));

  AutofillWalletCredentialSpecifics specifics =
      AutofillWalletCredentialSpecificsFromStructData(*server_cvc);

  EXPECT_TRUE(bridge()->IsEntityDataValid(SpecificsToEntity(specifics)));
}

TEST_F(AutofillWalletCredentialSyncBridgeTest, IsEntityDataValid_InValidData) {
  // Scenario 1: No instrument_id
  sync_pb::AutofillWalletCredentialSpecifics wallet_credential_specifics;
  wallet_credential_specifics.set_cvc("890");
  wallet_credential_specifics.set_last_updated_time_unix_epoch_millis(
      base::Milliseconds(25000).InMilliseconds());

  EXPECT_FALSE(bridge()->IsEntityDataValid(
      SpecificsToEntity(wallet_credential_specifics)));

  // Scenario 2: No CVC
  wallet_credential_specifics.set_instrument_id("123");
  wallet_credential_specifics.clear_cvc();

  EXPECT_FALSE(bridge()->IsEntityDataValid(
      SpecificsToEntity(wallet_credential_specifics)));

  // Scenario 3: No timestamp
  wallet_credential_specifics.set_cvc("890");
  wallet_credential_specifics.clear_last_updated_time_unix_epoch_millis();

  EXPECT_FALSE(bridge()->IsEntityDataValid(
      SpecificsToEntity(wallet_credential_specifics)));
}

}  // namespace autofill
