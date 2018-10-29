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
#include "base/message_loop/message_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
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
using syncer::EntityDataPtr;
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
    EXPECT_TRUE(table()->UpdateModelTypeState(syncer::AUTOFILL_WALLET_DATA,
                                              model_type_state));
    bridge_.reset(new AutofillWalletSyncBridge(
        active_callback_.Get(), mock_processor_.CreateForwardingProcessor(),
        UseFullSync(), &backend_));
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
    real_processor_->OnUpdateReceived(state, initial_updates);
  }

  void ExpectAddressesDiffInHistograms(int added, int removed) {
    histogram_tester_.ExpectUniqueSample("Autofill.WalletAddressesAdded",
                                         /*bucket=*/added,
                                         /*count=*/1);
    histogram_tester_.ExpectUniqueSample("Autofill.WalletAddressesRemoved",
                                         /*bucket=*/removed,
                                         /*count=*/1);
    histogram_tester_.ExpectUniqueSample(
        "Autofill.WalletAddressesAddedOrRemoved",
        /*bucket=*/added + removed,
        /*count=*/1);
  }

  void ExpectNoHistogramsForAddressesDiff() {
    histogram_tester_.ExpectTotalCount("Autofill.WalletAddressesAdded", 0);
    histogram_tester_.ExpectTotalCount("Autofill.WalletAddressesRemoved", 0);
    histogram_tester_.ExpectTotalCount("Autofill.WalletAddressesAddedOrRemoved",
                                       0);
  }

  void ExpectCardsDiffInHistograms(int added, int removed) {
    histogram_tester_.ExpectUniqueSample("Autofill.WalletCardsAdded",
                                         /*bucket=*/added,
                                         /*count=*/1);
    histogram_tester_.ExpectUniqueSample("Autofill.WalletCardsRemoved",
                                         /*bucket=*/removed,
                                         /*count=*/1);
    histogram_tester_.ExpectUniqueSample("Autofill.WalletCardsAddedOrRemoved",
                                         /*bucket=*/added + removed,
                                         /*count=*/1);
  }

  void ExpectNoHistogramsForCardsDiff() {
    histogram_tester_.ExpectTotalCount("Autofill.WalletCardsAdded", 0);
    histogram_tester_.ExpectTotalCount("Autofill.WalletCardsRemoved", 0);
    histogram_tester_.ExpectTotalCount("Autofill.WalletCardsAddedOrRemoved", 0);
  }

  EntityData SpecificsToEntity(const AutofillWalletSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_wallet() = specifics;
    data.client_tag_hash = syncer::GenerateSyncableHash(
        syncer::AUTOFILL_WALLET_DATA, bridge()->GetClientTag(data));
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

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const AutofillWalletSpecifics& specifics) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics).PassToPtr();
    return data;
  }

  AutofillWalletSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AutofillTable* table() { return &table_; }

  MockAutofillWebDataBackend* backend() { return &backend_; }

  base::MockCallback<base::RepeatingCallback<void(bool)>>* active_callback() {
    return &active_callback_;
  };

  virtual bool UseFullSync() { return true; }

 private:
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletSyncBridge> bridge_;
  base::HistogramTester histogram_tester_;
  NiceMock<base::MockCallback<base::RepeatingCallback<void(bool)>>>
      active_callback_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletSyncBridgeTest);
};

// The following 3 tests make sure client tags stay stable.
TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForAddress) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kAddr1SyncTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForCard) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kCard1SyncTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForCustomerData) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForPaymentsCustomerData(
          kCustomerDataSyncTag);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kCustomerDataSyncTag);
}

// The following 3 tests make sure storage keys stay stable.
TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForAddress) {
  AutofillWalletSpecifics specifics1 =
      CreateAutofillWalletSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics1)),
            kAddr1SpecificsId);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForCard) {
  AutofillWalletSpecifics specifics2 =
      CreateAutofillWalletSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics2)),
            kCard1SpecificsId);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForCustomerData) {
  AutofillWalletSpecifics specifics3 =
      CreateAutofillWalletSpecificsForPaymentsCustomerData(kCustomerDataId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics3)),
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
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(
                              AddChange(address2.guid(), address2)));
  EXPECT_CALL(*backend(),
              NotifyOfAutofillProfileChanged(RemoveChange(address1.guid())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card2.guid(), card2)));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(card1.guid())));
  StartSyncing({profile_specifics2, card_specifics2, customer_data_specifics});

  // Only the server card should be present on the client.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics2),
                                   EqualsSpecifics(card_specifics2),
                                   EqualsSpecifics(customer_data_specifics)));
  ExpectAddressesDiffInHistograms(/*added=*/1, /*removed=*/1);
  ExpectCardsDiffInHistograms(/*added=*/1, /*removed=*/1);
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
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics),
                                   EqualsSpecifics(customer_data_specifics)));
  ExpectNoHistogramsForAddressesDiff();
  ExpectNoHistogramsForCardsDiff();
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
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(_)).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(_)).Times(0);
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics2});

  // Only the server card should be present on the client.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics),
                                   EqualsSpecifics(customer_data_specifics2)));
  ExpectAddressesDiffInHistograms(/*added=*/0, /*removed=*/0);
  ExpectCardsDiffInHistograms(/*added=*/0, /*removed=*/0);
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
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(
                              RemoveChange(local_profile.guid())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(local_card.guid())));
  StartSyncing({});

  EXPECT_TRUE(GetAllLocalData().empty());
  ExpectAddressesDiffInHistograms(/*added=*/0, /*removed=*/1);
  ExpectCardsDiffInHistograms(/*added=*/0, /*removed=*/1);
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
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(_)).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(_)).Times(0);
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics),
                                   EqualsSpecifics(customer_data_specifics)));
  ExpectAddressesDiffInHistograms(/*added=*/0, /*removed=*/0);
  ExpectCardsDiffInHistograms(/*added=*/0, /*removed=*/0);
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

  // Create one of the same profiles and a different card on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(profile, &profile_specifics);
  // The Amex card has different values for the relevant fields.
  CreditCard card2 = test::GetMaskedServerCardAmex();
  AutofillWalletSpecifics card2_specifics;
  SetAutofillWalletSpecificsFromServerCard(card2, &card2_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(),
              NotifyOfAutofillProfileChanged(RemoveChange(profile2.guid())));
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(RemoveChange(card.guid())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card2.guid(), card2)));
  StartSyncing({profile_specifics, card2_specifics});

  // Make sure that the client only has the data from the server.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card2_specifics)));
  ExpectAddressesDiffInHistograms(/*added=*/0, /*removed=*/1);
  ExpectCardsDiffInHistograms(/*added=*/1, /*removed=*/1);
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
  EXPECT_FALSE(card.billing_address_id().empty());
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

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(
                              RemoveChange(local_profile.guid())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(local_card.guid())));
  // Passing in a non-null metadata change list indicates to the bridge that
  // sync is stopping because it was disabled.
  bridge()->ApplyStopSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>());

  EXPECT_TRUE(GetAllLocalData().empty());
  ExpectAddressesDiffInHistograms(/*added=*/0, /*removed=*/1);
  ExpectCardsDiffInHistograms(/*added=*/0, /*removed=*/1);
}

TEST_F(AutofillWalletSyncBridgeTest, ApplyStopSyncChanges_KeepData) {
  // Create one profile and one card on the client.
  AutofillProfile local_profile = test::GetServerProfile();
  table()->SetServerProfiles({local_profile});
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(_)).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(_)).Times(0);

  // Passing in a non-null metadata change list indicates to the bridge that
  // sync is stopping but the data type is not disabled.
  bridge()->ApplyStopSyncChanges(/*delete_metadata_change_list=*/nullptr);

  EXPECT_FALSE(GetAllLocalData().empty());
  ExpectNoHistogramsForAddressesDiff();
  ExpectNoHistogramsForCardsDiff();
}

TEST_F(AutofillWalletSyncBridgeTest, NotifiesWhenActivelySyncing) {
  testing::InSequence seq;

  ResetProcessor();

  EXPECT_CALL(*active_callback(), Run(true));
  ResetBridge(/*initial_sync_done=*/true);

  // Start and stop sync to check that we notify the callback.
  StartSyncing({});

  EXPECT_CALL(*active_callback(), Run(false));
  // Stopping sync with change list to indicate that the type is disabled.
  bridge()->ApplyStopSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>());

  EXPECT_CALL(*active_callback(), Run(true));
  // Start and stop sync again to make sure we notify the callback again.
  StartSyncing({});
  // Passing in a non-null metadata change list indicates to the bridge that
  // sync is stopping but the data type is not disabled, so we should not get
  // a callback.
  bridge()->ApplyStopSyncChanges(/*delete_metadata_change_list=*/nullptr);
}

class AutofillWalletEphemeralSyncBridgeTest
    : public AutofillWalletSyncBridgeTest {
 public:
  AutofillWalletEphemeralSyncBridgeTest() {}
  ~AutofillWalletEphemeralSyncBridgeTest() override {}

  bool UseFullSync() override { return false; }
};

// Tests that when the server sends no cards, the client should delete all it's
// existing data.
TEST_F(AutofillWalletEphemeralSyncBridgeTest, MergeSyncData_NoWalletCard) {
  // Create one card on the client.
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(local_card.guid())));
  StartSyncing({});

  EXPECT_TRUE(GetAllLocalData().empty());
  ExpectCardsDiffInHistograms(/*added=*/0, /*removed=*/1);
}

// Test that when the server sends the same card as the client has, nothing
// changes on the client.
TEST_F(AutofillWalletEphemeralSyncBridgeTest, MergeSyncData_SameWalletCard) {
  // Create one card on the client.
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});

  // Create the same card on the server.
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged(_)).Times(0);
  StartSyncing({card_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
  ExpectCardsDiffInHistograms(/*added=*/0, /*removed=*/0);
}

// Tests that when a new wallet card is sent by the server, the client only
// keeps the new card.
TEST_F(AutofillWalletEphemeralSyncBridgeTest, MergeSyncData_NewWalletCard) {
  // Create one card on the client.
  CreditCard card1 = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card1});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);

  // Create a different card on the server.
  CreditCard card2 = test::GetMaskedServerCardAmex();
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(card1.guid())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card2.guid(), card2)));
  StartSyncing({card_specifics2, customer_data_specifics});

  // Only the server card should be present on the client.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics2),
                                   EqualsSpecifics(customer_data_specifics)));
  ExpectCardsDiffInHistograms(/*added=*/1, /*removed=*/1);
}

// Tests that when a new wallet card and new wallet address are sent by the
// server, the client only keeps the new card and disregards the address.
TEST_F(AutofillWalletEphemeralSyncBridgeTest,
       MergeSyncData_AddressesAreDropped) {
  // Create one card on the client.
  CreditCard card1 = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card1});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);

  // Create a new profile and a different card on the server.
  AutofillProfile address = test::GetServerProfile();
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(address, &profile_specifics);
  CreditCard card2 = test::GetMaskedServerCardAmex();
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);

  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged(_)).Times(0);
  StartSyncing({profile_specifics, card_specifics2, customer_data_specifics});

  // Only the server card should be present on the client; the server profile is
  // ignored.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics2),
                                   EqualsSpecifics(customer_data_specifics)));
  // Nothing gets recorded for addresses - they are completely disregarded.
  ExpectNoHistogramsForAddressesDiff();
}

}  // namespace autofill
