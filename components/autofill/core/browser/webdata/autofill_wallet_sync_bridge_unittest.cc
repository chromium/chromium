// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/test_matchers.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using autofill::AutofillProfileChange;
using autofill::CreditCardChange;
using base::ScopedTempDir;
using sync_pb::AutofillWalletSpecifics;
using sync_pb::EntityMetadata;
using sync_pb::ModelTypeState;
using syncer::DataBatch;
using syncer::EntityChange;
using syncer::EntityData;
using syncer::HasInitialSyncDone;
using syncer::KeyAndData;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;
using testing::_;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

// Base64 encodings of the server IDs, used as ids in WalletMetadataSpecifics
// (these are suitable for syncing, because they are valid UTF-8).
const char kAddr1SpecificsId[] = "YWRkcjHvv74=";
const char kCard1SpecificsId[] = "Y2FyZDHvv74=";

// Represents a Payments customer id.
const char kCustomerDataId[] = "deadbeef";
const char kCustomerDataId2[] = "deadcafe";

// Unique sync tags for the server IDs.
const char kAddr1SyncTag[] = "YWRkcjHvv74=";
const char kCard1SyncTag[] = "Y2FyZDHvv74=";
const char kCustomerDataSyncTag[] = "deadbeef";

const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

void ExtractAutofillWalletSpecificsFromDataBatch(
    std::unique_ptr<DataBatch> batch,
    std::vector<AutofillWalletSpecifics>* output) {
  while (batch->HasNext()) {
    const KeyAndData& data_pair = batch->Next();
    output->push_back(data_pair.second->specifics.autofill_wallet());
  }
}

std::string WalletMaskedCreditCardSpecificsAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  std::ostringstream output;
  output << "[id: " << specifics.masked_card().id()
         << ", type: " << static_cast<int>(specifics.type())
         << ", name_on_card: " << specifics.masked_card().name_on_card()
         << ", type: " << specifics.masked_card().type()
         << ", last_four: " << specifics.masked_card().last_four()
         << ", exp_month: " << specifics.masked_card().exp_month()
         << ", exp_year: " << specifics.masked_card().exp_year()
         << ", billing_address_id: "
         << specifics.masked_card().billing_address_id()
         << ", card_class: " << specifics.masked_card().card_class()
         << ", bank_name: " << specifics.masked_card().bank_name() << "]";
  return output.str();
}

std::string WalletPostalAddressSpecificsAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  std::ostringstream output;
  output << "[id: " << specifics.address().id()
         << ", type: " << static_cast<int>(specifics.type())
         << ", recipient_name: " << specifics.address().recipient_name()
         << ", company_name: " << specifics.address().company_name()
         << ", street_address: "
         << (specifics.address().street_address_size()
                 ? specifics.address().street_address(0)
                 : "")
         << ", address_1: " << specifics.address().address_1()
         << ", address_2: " << specifics.address().address_2()
         << ", address_3: " << specifics.address().address_3()
         << ", postal_code: " << specifics.address().postal_code()
         << ", country_code: " << specifics.address().country_code()
         << ", phone_number: " << specifics.address().phone_number()
         << ", sorting_code: " << specifics.address().sorting_code() << "]";
  return output.str();
}

std::string AutofillWalletSpecificsAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  switch (specifics.type()) {
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_MASKED_CREDIT_CARD:
      return WalletMaskedCreditCardSpecificsAsDebugString(specifics);
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_POSTAL_ADDRESS:
      return WalletPostalAddressSpecificsAsDebugString(specifics);
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_CUSTOMER_DATA:
      return "CustomerData";
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_CREDIT_CARD_CLOUD_TOKEN_DATA:
      // TODO(crbug.com/1020740): Implement for this type.
      return "CloudTokenData";
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_UNKNOWN:
      return "Unknown";
  }
}

MATCHER_P(EqualsSpecifics, expected, "") {
  if (arg.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "entry\n"
                     << AutofillWalletSpecificsAsDebugString(arg) << "\n"
                     << "did not match expected\n"
                     << AutofillWalletSpecificsAsDebugString(expected);
    return false;
  }
  return true;
}

MATCHER_P(RemoveChange, key, "") {
  if (arg.type() != GenericAutofillChange<std::string>::REMOVE) {
    *result_listener << "type " << arg.type() << " is not REMOVE";
    return false;
  }
  if (arg.key() != key) {
    *result_listener << "key " << arg.key() << " does not match expected "
                     << key;
  }
  return true;
}

MATCHER_P2(AddChange, key, data, "") {
  if (arg.type() != GenericAutofillChange<std::string>::ADD) {
    *result_listener << "type " << arg.type() << " is not ADD";
    return false;
  }
  if (arg.key() != key) {
    *result_listener << "key " << arg.key() << " does not match expected "
                     << key;
  }
  if (*arg.data_model() != data) {
    *result_listener << "data " << *arg.data_model()
                     << " does not match expected " << data;
  }
  return true;
}

}  // namespace

class AutofillWalletSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletSyncBridgeTest() {}
  ~AutofillWalletSyncBridgeTest() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(*backend(), GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
    // Fake that initial sync has been done (so that the bridge immediately
    // records metrics).
    ResetBridge(/*initial_sync_done=*/true);
  }

  void ResetProcessor() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::AUTOFILL_WALLET_DATA, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge(bool initial_sync_done) {
    ModelTypeState model_type_state;
    model_type_state.set_initial_sync_done(initial_sync_done);
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::AUTOFILL_WALLET_DATA));
    EXPECT_TRUE(table()->UpdateModelTypeState(syncer::AUTOFILL_WALLET_DATA,
                                              model_type_state));
    bridge_ = std::make_unique<AutofillWalletSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  void StartSyncing(
      const std::vector<AutofillWalletSpecifics>& remote_data = {}) {
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

    // Initialize the processor with initial_sync_done.
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    state.mutable_progress_marker()
        ->mutable_gc_directive()
        ->set_version_watermark(1);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillWalletSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates));
  }

  void ExpectCountsOfWalletMetadataInDB(unsigned int cards_count,
                                        unsigned int addresses_count) {
    std::map<std::string, AutofillMetadata> cards_metadata;
    ASSERT_TRUE(table()->GetServerCardsMetadata(&cards_metadata));
    EXPECT_EQ(cards_count, cards_metadata.size());

    std::map<std::string, AutofillMetadata> addresses_metadata;
    ASSERT_TRUE(table()->GetServerAddressesMetadata(&addresses_metadata));
    EXPECT_EQ(addresses_count, addresses_metadata.size());
  }

  std::unique_ptr<EntityData> SpecificsToEntity(
      const AutofillWalletSpecifics& specifics) {
    auto data = std::make_unique<EntityData>();
    *data->specifics.mutable_autofill_wallet() = specifics;
    data->client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL_WALLET_DATA, bridge()->GetClientTag(*data));
    return data;
  }

  std::vector<AutofillWalletSpecifics> GetAllLocalData() {
    std::vector<AutofillWalletSpecifics> data;
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetAllDataForTesting(base::BindLambdaForTesting(
        [&loop, &data](std::unique_ptr<DataBatch> batch) {
          ExtractAutofillWalletSpecificsFromDataBatch(std::move(batch), &data);
          loop.Quit();
        }));
    loop.Run();
    return data;
  }

  std::unique_ptr<syncer::UpdateResponseData> SpecificsToUpdateResponse(
      const AutofillWalletSpecifics& specifics) {
    auto data = std::make_unique<syncer::UpdateResponseData>();
    data->entity = SpecificsToEntity(specifics);
    return data;
  }

  AutofillWalletSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AutofillTable* table() { return &table_; }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletSyncBridgeTest);
};

// The following 3 tests make sure client tags stay stable.
TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForAddress) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetClientTag(*SpecificsToEntity(specifics)),
            kAddr1SyncTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForCard) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetClientTag(*SpecificsToEntity(specifics)),
            kCard1SyncTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForCustomerData) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForPaymentsCustomerData(
          kCustomerDataSyncTag);
  EXPECT_EQ(bridge()->GetClientTag(*SpecificsToEntity(specifics)),
            kCustomerDataSyncTag);
}

// The following 3 tests make sure storage keys stay stable.
TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForAddress) {
  AutofillWalletSpecifics specifics1 =
      CreateAutofillWalletSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(*SpecificsToEntity(specifics1)),
            kAddr1SpecificsId);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForCard) {
  AutofillWalletSpecifics specifics2 =
      CreateAutofillWalletSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(*SpecificsToEntity(specifics2)),
            kCard1SpecificsId);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForCustomerData) {
  AutofillWalletSpecifics specifics3 =
      CreateAutofillWalletSpecificsForPaymentsCustomerData(kCustomerDataId);
  EXPECT_EQ(bridge()->GetStorageKey(*SpecificsToEntity(specifics3)),
            kCustomerDataId);
}

TEST_F(AutofillWalletSyncBridgeTest,
       GetAllDataForDebugging_ShouldReturnAllData) {
  AutofillProfile address1 = test::GetServerProfile();
  AutofillProfile address2 = test::GetServerProfile2();
  table()->SetServerProfiles({address1, address2});
  CreditCard card1 = test::GetMaskedServerCard();
  CreditCard card2 = test::GetMaskedServerCardAmex();
  table()->SetServerCreditCards({card1, card2});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);

  AutofillWalletSpecifics profile_specifics1;
  SetAutofillWalletSpecificsFromServerProfile(address1, &profile_specifics1);
  AutofillWalletSpecifics profile_specifics2;
  SetAutofillWalletSpecificsFromServerProfile(address2, &profile_specifics2);
  AutofillWalletSpecifics card_specifics1;
  SetAutofillWalletSpecificsFromServerCard(card1, &card_specifics1);
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics1),
                                   EqualsSpecifics(profile_specifics2),
                                   EqualsSpecifics(card_specifics1),
                                   EqualsSpecifics(card_specifics2),
                                   EqualsSpecifics(customer_data_specifics)));
}

// Tests that when a new wallet card and new wallet address are sent by the
// server, the client only keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NewWalletAddressAndCard) {
  // Create one profile and one card on the client.
  AutofillProfile address1 = test::GetServerProfile();
  table()->SetServerProfiles({address1});
  CreditCard card1 = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card1});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);

  // Create a different profile and a different card on the server.
  AutofillProfile address2 = test::GetServerProfile2();
  AutofillWalletSpecifics profile_specifics2;
  SetAutofillWalletSpecificsFromServerProfile(address2, &profile_specifics2);
  CreditCard card2 = test::GetMaskedServerCardAmex();
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(
                              AddChange(address2.server_id(), address2)));
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(
                              RemoveChange(address1.server_id())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card2.server_id(), card2)));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(card1.server_id())));
  StartSyncing({profile_specifics2, card_specifics2, customer_data_specifics});

  // This bridge does not store metadata, i.e. billing_address_id. Strip it
  // off so that the expectations below pass.
  card_specifics2.mutable_masked_card()->set_billing_address_id(std::string());

  // Only the server card should be present on the client.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics2),
                                   EqualsSpecifics(card_specifics2),
                                   EqualsSpecifics(customer_data_specifics)));
}

// Tests that in initial sync, no metrics are recorded for new addresses and
// cards.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeSyncData_NewWalletAddressAndCardNoMetricsInitialSync) {
  ResetProcessor();
  ResetBridge(/*initial_sync_done=*/false);

  // Create a data set on the server.
  AutofillProfile address = test::GetServerProfile();
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(address, &profile_specifics);
  CreditCard card = test::GetMaskedServerCard();
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics});

  ExpectCountsOfWalletMetadataInDB(/*cards_count=*/0u, /*address_count=*/0u);

  // This bridge does not store metadata, i.e. billing_address_id. Strip it
  // off so that the expectations below pass.
  card_specifics.mutable_masked_card()->set_billing_address_id(std::string());

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics),
                                   EqualsSpecifics(customer_data_specifics)));
}

// Tests that when a new payments customer data is sent by the server, the
// client only keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NewPaymentsCustomerData) {
  // Create one profile, one card and one customer data entry on the client.
  AutofillProfile address = test::GetServerProfile();
  table()->SetServerProfiles({address});
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data1{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data1);

  // Create a different customer data entry on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(address, &profile_specifics);
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
  PaymentsCustomerData customer_data2{/*customer_id=*/kCustomerDataId2};
  AutofillWalletSpecifics customer_data_specifics2;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data2,
                                                     &customer_data_specifics2);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(_)).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(_)).Times(0);
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics2});

  // Only the server card should be present on the client.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics),
                                   EqualsSpecifics(customer_data_specifics2)));
}

// Tests that when the server sends no cards or address, the client should
// delete all it's existing data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NoWalletAddressOrCard) {
  // Create one profile and one card on the client.
  AutofillProfile local_profile = test::GetServerProfile();
  table()->SetServerProfiles({local_profile});
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(
                              RemoveChange(local_profile.server_id())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(local_card.server_id())));
  StartSyncing({});

  // This bridge should not touch the metadata; should get deleted by the
  // metadata bridge.
  ExpectCountsOfWalletMetadataInDB(/*cards_count=*/1u, /*address_count=*/1u);

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Test that when the server sends the same address and card as the client has,
// nothing changes on the client.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeSyncData_SameWalletAddressAndCardAndCustomerData) {
  // Create one profile and one card on the client.
  AutofillProfile profile = test::GetServerProfile();
  table()->SetServerProfiles({profile});
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);

  // Create the same profile and card on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(profile, &profile_specifics);
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker on the client.
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(_)).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(_)).Times(0);
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics),
                                   EqualsSpecifics(customer_data_specifics)));
}

// Tests that when there are multiple changes happening at the same time, the
// data from the server is what the client ends up with.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeSyncData_AddRemoveAndPreserveWalletAddressAndCard) {
  // Create two profile and one card on the client.
  AutofillProfile profile = test::GetServerProfile();
  AutofillProfile profile2 = test::GetServerProfile2();
  table()->SetServerProfiles({profile, profile2});
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);

  // Create one of the same profiles and a different card on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(profile, &profile_specifics);
  // The Amex card has different values for the relevant fields.
  CreditCard card2 = test::GetMaskedServerCardAmex();
  AutofillWalletSpecifics card2_specifics;
  SetAutofillWalletSpecificsFromServerCard(card2, &card2_specifics);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(
                              RemoveChange(profile2.server_id())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(card.server_id())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card2.server_id(), card2)));
  StartSyncing({profile_specifics, card2_specifics, customer_data_specifics});

  // This bridge does not store metadata, i.e. billing_address_id. Strip it
  // off so that the expectations below pass.
  card2_specifics.mutable_masked_card()->set_billing_address_id(std::string());

  // Make sure that the client only has the data from the server.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card2_specifics),
                                   EqualsSpecifics(customer_data_specifics)));
}

// Test that all field values for a address sent form the server are copied on
// the address on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_SetsAllWalletAddressData) {
  // Create a profile to be synced from the server.
  AutofillProfile profile = test::GetServerProfile();
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(profile, &profile_specifics);

  StartSyncing({profile_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics)));

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  table()->GetServerProfiles(&profiles);
  ASSERT_EQ(1U, profiles.size());

  // Make sure that all the data was set properly.
  EXPECT_EQ(profile.GetRawInfo(NAME_FULL), profiles[0]->GetRawInfo(NAME_FULL));
  EXPECT_EQ(profile.GetRawInfo(COMPANY_NAME),
            profiles[0]->GetRawInfo(COMPANY_NAME));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            profiles[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_STATE),
            profiles[0]->GetRawInfo(ADDRESS_HOME_STATE));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_CITY),
            profiles[0]->GetRawInfo(ADDRESS_HOME_CITY));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY),
            profiles[0]->GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_ZIP),
            profiles[0]->GetRawInfo(ADDRESS_HOME_ZIP));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_COUNTRY),
            profiles[0]->GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
            profiles[0]->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE),
            profiles[0]->GetRawInfo(ADDRESS_HOME_SORTING_CODE));
  EXPECT_EQ(profile.language_code(), profiles[0]->language_code());

  // Also make sure that those types are not empty, to exercice all the code
  // paths.
  EXPECT_FALSE(profile.GetRawInfo(NAME_FULL).empty());
  EXPECT_FALSE(profile.GetRawInfo(COMPANY_NAME).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_STATE).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_CITY).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_ZIP).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_COUNTRY).empty());
  EXPECT_FALSE(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE).empty());
  EXPECT_FALSE(profile.language_code().empty());
}

// Test that all field values for a card sent form the server are copied on the
// card on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_SetsAllWalletCardData) {
  // Create a card to be synced from the server.
  CreditCard card = test::GetMaskedServerCard();
  // Add this value type as it is not added by default but should be synced.
  card.set_bank_name("The Bank");
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);

  StartSyncing({card_specifics});

  // This bridge does not store metadata, i.e. billing_address_id. Strip it
  // off so that the expectations below pass.
  card.set_billing_address_id(std::string());
  card_specifics.mutable_masked_card()->set_billing_address_id(std::string());

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));

  std::vector<std::unique_ptr<CreditCard>> cards;
  table()->GetServerCreditCards(&cards);
  ASSERT_EQ(1U, cards.size());

  // Make sure that all the data was set properly.
  EXPECT_EQ(card.network(), cards[0]->network());
  EXPECT_EQ(card.LastFourDigits(), cards[0]->LastFourDigits());
  EXPECT_EQ(card.expiration_month(), cards[0]->expiration_month());
  EXPECT_EQ(card.expiration_year(), cards[0]->expiration_year());
  EXPECT_EQ(card.billing_address_id(), cards[0]->billing_address_id());
  EXPECT_EQ(card.card_type(), cards[0]->card_type());
  EXPECT_EQ(card.bank_name(), cards[0]->bank_name());

  // Also make sure that those types are not empty, to exercice all the code
  // paths.
  EXPECT_FALSE(card.network().empty());
  EXPECT_FALSE(card.LastFourDigits().empty());
  EXPECT_NE(0, card.expiration_month());
  EXPECT_NE(0, card.expiration_year());
  EXPECT_NE(CreditCard::CARD_TYPE_UNKNOWN, card.card_type());
  EXPECT_FALSE(card.bank_name().empty());
}

TEST_F(AutofillWalletSyncBridgeTest, LoadMetadataCalled) {
  EXPECT_TRUE(table()->UpdateSyncMetadata(syncer::AUTOFILL_WALLET_DATA, "key",
                                          EntityMetadata()));

  ResetProcessor();
  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/HasInitialSyncDone(),
                                    /*entities=*/SizeIs(1))));
  ResetBridge(/*initial_sync_done=*/true);
}

TEST_F(AutofillWalletSyncBridgeTest, ApplyStopSyncChanges_ClearAllData) {
  // Create one profile and one card on the client.
  AutofillProfile local_profile = test::GetServerProfile();
  table()->SetServerProfiles({local_profile});
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(_)).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(_)).Times(0);

  // Passing in a non-null metadata change list indicates to the bridge that
  // sync is stopping because it was disabled.
  bridge()->ApplyStopSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>());

  // This bridge should not touch the metadata; should get deleted by the
  // metadata bridge.
  ExpectCountsOfWalletMetadataInDB(/*cards_count=*/1u, /*address_count=*/1u);

  EXPECT_TRUE(GetAllLocalData().empty());
}

TEST_F(AutofillWalletSyncBridgeTest, ApplyStopSyncChanges_KeepData) {
  // Create one profile and one card on the client.
  AutofillProfile local_profile = test::GetServerProfile();
  table()->SetServerProfiles({local_profile});
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});

  // We do not write to DB at all, so we should not commit any changes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(_)).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(_)).Times(0);

  // Passing in a non-null metadata change list indicates to the bridge that
  // sync is stopping but the data type is not disabled.
  bridge()->ApplyStopSyncChanges(/*delete_metadata_change_list=*/nullptr);

  EXPECT_FALSE(GetAllLocalData().empty());
}

}  // namespace autofill
