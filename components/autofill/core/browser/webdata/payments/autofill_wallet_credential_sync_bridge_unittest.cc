// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/autofill_wallet_credential_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/autofill_wallet_credential_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using sync_pb::AutofillWalletCredentialSpecifics;
using sync_pb::DataTypeState;
using syncer::EntityData;
using syncer::MockDataTypeLocalChangeProcessor;
using testing::NiceMock;
using testing::Return;

std::vector<ServerCvc> ExtractServerCvcDataFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<ServerCvc> server_cvc_data;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    server_cvc_data.push_back(
        AutofillWalletCvcStructDataFromWalletCredentialSpecifics(
            data_pair.second->specifics.autofill_wallet_credential()));
  }
  return server_cvc_data;
}

class AutofillWalletCredentialSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletCredentialSyncBridgeTest()
      : encryptor_(os_crypt_async::GetTestEncryptorForTesting()) {}

  void SetUp() override {
    db_.AddTable(&sync_metadata_table_);
    db_.AddTable(&table_);
    db_.Init(base::FilePath(WebDatabase::kInMemoryPath), &encryptor_);
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
    bridge_ = std::make_unique<AutofillWalletCredentialSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  void ResetProcessor() {
    real_processor_ = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
        syncer::AUTOFILL_WALLET_CREDENTIAL,
        /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  std::vector<ServerCvc> GetAllServerCvcDataFromTable() {
    // In tests, it's more convenient to work without `std::unique_ptr`.
    std::vector<ServerCvc> server_cvc_data;
    for (const std::unique_ptr<ServerCvc>& data : table()->GetAllServerCvcs()) {
      server_cvc_data.push_back(std::move(*data));
    }
    return server_cvc_data;
  }

  EntityData SpecificsToEntity(
      const AutofillWalletCredentialSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_wallet_credential() = specifics;
    return data;
  }

  AutofillWalletCredentialSyncBridge* bridge() { return bridge_.get(); }

  PaymentsAutofillTable* table() { return &table_; }

  MockAutofillWebDataBackend& backend() { return backend_; }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  void StartSyncing(
      const std::vector<AutofillWalletCredentialSpecifics>& remote_data = {}) {
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();
    // ClientTagBasedDataTypeProcessor requires connecting before other
    // interactions with the worker happen.
    real_processor_->ConnectSync(
        std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());
    // Initialize the processor with the initial sync already done.
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillWalletCredentialSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates),
                                      /*gc_directive=*/std::nullopt);
  }

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const AutofillWalletCredentialSpecifics& specifics) {
    EntityData entity_data;
    *entity_data.specifics.mutable_autofill_wallet_credential() = specifics;
    entity_data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL_WALLET_CREDENTIAL,
        bridge()->GetClientTag(entity_data));

    syncer::UpdateResponseData update_response_data;
    update_response_data.entity = std::move(entity_data);
    return update_response_data;
  }

 private:
  const os_crypt_async::Encryptor encryptor_;
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillSyncMetadataTable sync_metadata_table_;
  PaymentsAutofillTable table_;
  WebDatabase db_;
  NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedDataTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletCredentialSyncBridge> bridge_;
  base::test::SingleThreadTaskEnvironment task_environment_;
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

// Test to verify full merge sync for the server cvc data.
// There is no existing server cvc data on the local storage.
TEST_F(AutofillWalletCredentialSyncBridgeTest, MergeFullSyncData) {
  syncer::EntityChangeList entity_change_list;
  const ServerCvc server_cvc =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      base::NumberToString(server_cvc.instrument_id),
      SpecificsToEntity(
          AutofillWalletCredentialSpecificsFromStructData(server_cvc))));

  EXPECT_EQ(bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                                        std::move(entity_change_list)),
            std::nullopt);
  EXPECT_THAT(GetAllServerCvcDataFromTable(),
              testing::UnorderedElementsAre(server_cvc));
}

// Test to verify incremental sync to add a server cvc.
// A server cvc on the local storage is added via MergeFullSync and then
// incremental sync is called with new unique server cvc data.
TEST_F(AutofillWalletCredentialSyncBridgeTest,
       ApplyIncrementalSyncChanges_AddCvc) {
  const ServerCvc server_cvc1 =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));

  StartSyncing({AutofillWalletCredentialSpecificsFromStructData(server_cvc1)});

  EXPECT_THAT(GetAllServerCvcDataFromTable(),
              testing::UnorderedElementsAre(server_cvc1));

  // Add a new server cvc.
  syncer::EntityChangeList entity_change_list;
  const ServerCvc server_cvc2 =
      ServerCvc(2, u"999", base::Time::UnixEpoch() + base::Milliseconds(50000));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      base::NumberToString(server_cvc2.instrument_id),
      SpecificsToEntity(
          AutofillWalletCredentialSpecificsFromStructData(server_cvc2))));

  // Expect no changes to the remote server credential data.
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_WALLET_CREDENTIAL));

  EXPECT_EQ(
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list)),
      std::nullopt);
  EXPECT_THAT(GetAllServerCvcDataFromTable(),
              testing::UnorderedElementsAre(server_cvc1, server_cvc2));
}

// Test to verify incremental sync to delete a server cvc.
// A server cvc on the local storage is added via MergeFullSync and then
// incremental sync is called to delete the existing server cvc.
TEST_F(AutofillWalletCredentialSyncBridgeTest,
       ApplyIncrementalSyncChanges_DeleteCvc) {
  const ServerCvc server_cvc1 =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));

  StartSyncing({AutofillWalletCredentialSpecificsFromStructData(server_cvc1)});

  EXPECT_THAT(GetAllServerCvcDataFromTable(),
              testing::UnorderedElementsAre(server_cvc1));

  // Delete an existing server cvc.
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateDelete(
      base::NumberToString(server_cvc1.instrument_id)));

  // Expect no changes to the remote server credential data.
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_WALLET_CREDENTIAL));

  EXPECT_EQ(
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list)),
      std::nullopt);
  EXPECT_THAT(GetAllServerCvcDataFromTable(), testing::IsEmpty());
}

// Test to verify incremental sync to update a server cvc.
// A server cvc on the local storage is added via MergeFullSync and then
// incremental sync is called to update the existing server cvc.
TEST_F(AutofillWalletCredentialSyncBridgeTest,
       ApplyIncrementalSyncChanges_UpdateCvc) {
  const ServerCvc server_cvc1 =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));

  StartSyncing({AutofillWalletCredentialSpecificsFromStructData(server_cvc1)});

  EXPECT_THAT(GetAllServerCvcDataFromTable(),
              testing::UnorderedElementsAre(server_cvc1));

  // Update the above CVC with new data and later timestamp.
  syncer::EntityChangeList entity_change_list;
  const ServerCvc server_cvc2 =
      ServerCvc(1, u"999", base::Time::UnixEpoch() + base::Milliseconds(50000));
  entity_change_list.push_back(syncer::EntityChange::CreateUpdate(
      base::NumberToString(server_cvc2.instrument_id),
      SpecificsToEntity(
          AutofillWalletCredentialSpecificsFromStructData(server_cvc2))));

  // Expect no changes to the remote server credential data.
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_WALLET_CREDENTIAL));

  EXPECT_EQ(
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list)),
      std::nullopt);
  EXPECT_THAT(GetAllServerCvcDataFromTable(),
              testing::UnorderedElementsAre(server_cvc2));
}

// Test to verify addition changes in server cvc on the local database are being
// sent to the Chrome sync server.
TEST_F(AutofillWalletCredentialSyncBridgeTest, ServerCvcChanged_Add) {
  StartSyncing({});

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(1);
  EXPECT_CALL(backend(), CommitChanges()).Times(0);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_CREDENTIAL))
      .Times(0);

  const ServerCvc server_cvc =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));
  const ServerCvcChange change = ServerCvcChange(
      ServerCvcChange::ADD, server_cvc.instrument_id, server_cvc);
  bridge()->ServerCvcChanged(change);
}

// Test to verify update changes in server cvc on the local database are being
// sent to the Chrome sync server.
TEST_F(AutofillWalletCredentialSyncBridgeTest, ServerCvcChanged_Update) {
  StartSyncing({});

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(1);
  EXPECT_CALL(backend(), CommitChanges()).Times(0);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_CREDENTIAL))
      .Times(0);

  const ServerCvc server_cvc =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));
  const ServerCvcChange change = ServerCvcChange(
      ServerCvcChange::UPDATE, server_cvc.instrument_id, server_cvc);
  bridge()->ServerCvcChanged(change);
}

// Test to verify deletion changes in server cvc on the local database are being
// sent to the Chrome sync server.
TEST_F(AutofillWalletCredentialSyncBridgeTest, ServerCvcChanged_Remove) {
  StartSyncing({});

  EXPECT_CALL(mock_processor(), Delete).Times(1);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges()).Times(0);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_CREDENTIAL))
      .Times(0);

  const ServerCvc server_cvc =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));
  const ServerCvcChange change = ServerCvcChange(
      ServerCvcChange::REMOVE, server_cvc.instrument_id, server_cvc);
  bridge()->ServerCvcChanged(change);
}

// Test to verify all the server cvc data is deleted/cleared when the sync is
// disabled.
TEST_F(AutofillWalletCredentialSyncBridgeTest, ApplyDisableSyncChanges) {
  const ServerCvc server_cvc =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));

  StartSyncing({AutofillWalletCredentialSpecificsFromStructData(server_cvc)});

  EXPECT_THAT(GetAllServerCvcDataFromTable(),
              testing::UnorderedElementsAre(server_cvc));
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOnAutofillChangedBySync(
                             syncer::AUTOFILL_WALLET_CREDENTIAL));

  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());

  EXPECT_TRUE(GetAllServerCvcDataFromTable().empty());
}

// Test to verify no deletion APIs are triggered in `ApplyDisableSyncChanges`
// when there is no server CVC data to delete.
TEST_F(AutofillWalletCredentialSyncBridgeTest,
       ApplyDisableSyncChanges_NoServerCvcPresent) {
  EXPECT_TRUE(GetAllServerCvcDataFromTable().empty());

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges()).Times(0);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_CREDENTIAL))
      .Times(0);

  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());

  EXPECT_TRUE(GetAllServerCvcDataFromTable().empty());
  EXPECT_FALSE(bridge()->change_processor()->GetError().has_value());
}

// Test to get all the server cvc data for a user which is filtered on the list
// of `instrument_id` provided.
TEST_F(AutofillWalletCredentialSyncBridgeTest, GetDataForCommit) {
  const ServerCvc server_cvc1 =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));
  const ServerCvc server_cvc2 =
      ServerCvc(2, u"890", base::Time::UnixEpoch() + base::Milliseconds(50000));
  table()->AddServerCvc(server_cvc1);
  table()->AddServerCvc(server_cvc2);

  // Synchronously get data of `server_cvc1`.
  std::vector<ServerCvc> server_cvc_from_get_data =
      ExtractServerCvcDataFromDataBatch(bridge()->GetDataForCommit(
          {base::NumberToString(server_cvc1.instrument_id)}));

  EXPECT_THAT(server_cvc_from_get_data, testing::ElementsAre(server_cvc1));
}

// Test to get all the server cvc data for a user while debugging.
TEST_F(AutofillWalletCredentialSyncBridgeTest, GetAllDataForDebugging) {
  const ServerCvc server_cvc1 =
      ServerCvc(1, u"123", base::Time::UnixEpoch() + base::Milliseconds(25000));
  const ServerCvc server_cvc2 =
      ServerCvc(2, u"890", base::Time::UnixEpoch() + base::Milliseconds(50000));
  table()->AddServerCvc(server_cvc1);
  table()->AddServerCvc(server_cvc2);

  std::vector<ServerCvc> server_cvc_from_get_all =
      ExtractServerCvcDataFromDataBatch(bridge()->GetAllDataForDebugging());

  EXPECT_THAT(server_cvc_from_get_all,
              testing::UnorderedElementsAre(server_cvc1, server_cvc2));
}

// Test to verify the non deletion/updation of the server cvc for the card with
// the `UPDATE` change tag.
TEST_F(AutofillWalletCredentialSyncBridgeTest,
       DoesNotUpdateOrDeleteServerCvcWhenCardUpdated) {
  const CreditCard card1 = test::GetMaskedServerCard();
  const CreditCard card2 = test::GetMaskedServerCard2();
  const ServerCvc server_cvc1 =
      ServerCvc{card1.instrument_id(), u"123",
                base::Time::UnixEpoch() + base::Milliseconds(25000)};
  const ServerCvc server_cvc2 =
      ServerCvc{card2.instrument_id(), u"890",
                base::Time::UnixEpoch() + base::Milliseconds(50000)};
  table()->AddServerCvc(server_cvc1);
  table()->AddServerCvc(server_cvc2);

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges()).Times(0);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_CREDENTIAL))
      .Times(0);

  bridge()->CreditCardChanged(
      CreditCardChange(CreditCardChange::UPDATE, card1.server_id(), card1));

  EXPECT_THAT(GetAllServerCvcDataFromTable(),
              testing::UnorderedElementsAre(server_cvc1, server_cvc2));
}

}  // namespace
}  // namespace autofill
