// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
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
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
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
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

// Represents a Payments customer id.
const char kCustomerDataId[] = "deadbeef";
const char kCustomerDataId2[] = "deadcafe";

// Unique client tags for the server data.
const char kAddr1ClientTag[] = "YWRkcjHvv74=";
const char kCard1ClientTag[] = "Y2FyZDHvv74=";
const char kCustomerDataClientTag[] = "deadbeef";
const char kCloudTokenDataClientTag[] = "token";

const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

const char kDefaultCacheGuid[] = "CacheGuid";

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
         << ", bank_name: " << specifics.masked_card().bank_name()
         << ", instrument_id: " << specifics.masked_card().instrument_id()
         << "]";
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

std::string WalletCreditCardCloudTokenDataSpecificsAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  std::ostringstream output;
  output << "[masked_card_id: " << specifics.cloud_token_data().masked_card_id()
         << ", suffix: " << specifics.cloud_token_data().suffix()
         << ", exp_month: " << specifics.cloud_token_data().exp_month()
         << ", exp_year: " << specifics.cloud_token_data().exp_year()
         << ", card_art_url: " << specifics.cloud_token_data().art_fife_url()
         << ", instrument_token: "
         << specifics.cloud_token_data().instrument_token() << "]";
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
      return WalletCreditCardCloudTokenDataSpecificsAsDebugString(specifics);
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

  AutofillWalletSyncBridgeTest(const AutofillWalletSyncBridgeTest&) = delete;
  AutofillWalletSyncBridgeTest& operator=(const AutofillWalletSyncBridgeTest&) =
      delete;

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
            syncer::AUTOFILL_WALLET_DATA, /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge(bool initial_sync_done) {
    ModelTypeState model_type_state;
    model_type_state.set_initial_sync_done(initial_sync_done);
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::AUTOFILL_WALLET_DATA));
    model_type_state.set_cache_guid(kDefaultCacheGuid);
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
    request.cache_guid = kDefaultCacheGuid;
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

    sync_pb::GarbageCollectionDirective gc_directive;
    gc_directive.set_version_watermark(1);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillWalletSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates),
                                      gc_directive);
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

  EntityData SpecificsToEntity(const AutofillWalletSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_wallet() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
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
    data.entity = SpecificsToEntity(specifics);
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
};

// The following 4 tests make sure client tags stay stable.
TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForAddress) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForAddress(kAddr1ClientTag);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kAddr1ClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForCard) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForCard(kCard1ClientTag);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kCard1ClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForCustomerData) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForPaymentsCustomerData(
          kCustomerDataClientTag);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kCustomerDataClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForCreditCardCloudTokenData) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForCreditCardCloudTokenData(
          kCloudTokenDataClientTag);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kCloudTokenDataClientTag);
}

// The following 4 tests make sure storage keys stay stable.
TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForAddress) {
  AutofillWalletSpecifics specifics1 =
      CreateAutofillWalletSpecificsForAddress(kAddr1ClientTag);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics1)),
            kAddr1ClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForCard) {
  AutofillWalletSpecifics specifics2 =
      CreateAutofillWalletSpecificsForCard(kCard1ClientTag);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics2)),
            kCard1ClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForCustomerData) {
  AutofillWalletSpecifics specifics3 =
      CreateAutofillWalletSpecificsForPaymentsCustomerData(
          kCustomerDataClientTag);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics3)),
            kCustomerDataClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForCreditCardCloudTokenData) {
  AutofillWalletSpecifics specifics4 =
      CreateAutofillWalletSpecificsForCreditCardCloudTokenData(
          kCloudTokenDataClientTag);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics4)),
            kCloudTokenDataClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest,
       GetAllDataForDebugging_ShouldReturnAllData) {
  // Create Wallet Data and store them to table.
  AutofillProfile address1 = test::GetServerProfile();
  AutofillProfile address2 = test::GetServerProfile2();
  table()->SetServerProfiles({address1, address2});
  CreditCard card1 = test::GetMaskedServerCard();
  // Set the card issuer to Google.
  card1.set_card_issuer(CreditCard::Issuer::GOOGLE);
  card1.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNENROLLED);
  card1.set_card_art_url(GURL("https://www.example.com/card.png"));
  CreditCard card2 = test::GetMaskedServerCardAmex();
  CreditCard card_with_nickname = test::GetMaskedServerCardWithNickname();
  card2.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  table()->SetServerCreditCards({card1, card2, card_with_nickname});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);
  CreditCardCloudTokenData data1 = test::GetCreditCardCloudTokenData1();
  CreditCardCloudTokenData data2 = test::GetCreditCardCloudTokenData2();
  table()->SetCreditCardCloudTokenData({data1, data2});

  AutofillWalletSpecifics profile_specifics1;
  SetAutofillWalletSpecificsFromServerProfile(address1, &profile_specifics1);
  AutofillWalletSpecifics profile_specifics2;
  SetAutofillWalletSpecificsFromServerProfile(address2, &profile_specifics2);
  AutofillWalletSpecifics card_specifics1;
  SetAutofillWalletSpecificsFromServerCard(card1, &card_specifics1);
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);
  AutofillWalletSpecifics card_specifics_with_nickname;
  SetAutofillWalletSpecificsFromServerCard(card_with_nickname,
                                           &card_specifics_with_nickname);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);
  AutofillWalletSpecifics cloud_token_data_specifics1;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      data1, &cloud_token_data_specifics1);
  AutofillWalletSpecifics cloud_token_data_specifics2;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      data2, &cloud_token_data_specifics2);

  // First ensure that specific fields in expected wallet specifics are set
  // correctly before we compare with local table.
  EXPECT_FALSE(card_specifics_with_nickname.masked_card().nickname().empty());
  EXPECT_TRUE(card_specifics2.masked_card().nickname().empty());
  EXPECT_EQ(sync_pb::CardIssuer::GOOGLE,
            card_specifics1.masked_card().card_issuer().issuer());
  EXPECT_EQ(sync_pb::CardIssuer::ISSUER_UNKNOWN,
            card_specifics2.masked_card().card_issuer().issuer());
  EXPECT_EQ(sync_pb::WalletMaskedCreditCard::UNENROLLED,
            card_specifics1.masked_card().virtual_card_enrollment_state());
  EXPECT_EQ(sync_pb::WalletMaskedCreditCard::ENROLLED,
            card_specifics2.masked_card().virtual_card_enrollment_state());
  EXPECT_EQ("https://www.example.com/card.png",
            card_specifics1.masked_card().card_art_url());
  EXPECT_TRUE(card_specifics2.masked_card().card_art_url().empty());

  // Read local Wallet Data from Autofill table, and compare with expected
  // wallet specifics.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(profile_specifics1),
                           EqualsSpecifics(profile_specifics2),
                           EqualsSpecifics(card_specifics1),
                           EqualsSpecifics(card_specifics2),
                           EqualsSpecifics(card_specifics_with_nickname),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics1),
                           EqualsSpecifics(cloud_token_data_specifics2)));
}

// Tests that when multiple credit card cloud token data have the same masked
// card id, the data can be obtained correctly.
TEST_F(AutofillWalletSyncBridgeTest,
       GetAllDataForDebugging_MultipleCloudTokenDataForOneCard) {
  CreditCardCloudTokenData data1 = test::GetCreditCardCloudTokenData1();
  CreditCardCloudTokenData data2 = test::GetCreditCardCloudTokenData2();
  // Make the masked card ids for both data the same one.
  data2.masked_card_id = data1.masked_card_id;
  table()->SetCreditCardCloudTokenData({data1, data2});

  AutofillWalletSpecifics cloud_token_data_specifics1;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      data1, &cloud_token_data_specifics1);
  AutofillWalletSpecifics cloud_token_data_specifics2;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      data2, &cloud_token_data_specifics2);

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(cloud_token_data_specifics1),
                           EqualsSpecifics(cloud_token_data_specifics2)));
}

// Tests that when a new wallet card and new wallet address are sent by the
// server, the client only keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NewWalletAddressAndCard) {
  // Create one profile and one card on the client.
  AutofillProfile address1 = test::GetServerProfile();
  table()->SetServerProfiles({address1});
  CreditCard card1 = test::GetMaskedServerCard();
  card1.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNENROLLED);
  card1.set_card_art_url(GURL("https://www.example.com/card.png"));
  table()->SetServerCreditCards({card1});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  // Create a different profile and a different card on the server.
  AutofillProfile address2 = test::GetServerProfile2();
  AutofillWalletSpecifics profile_specifics2;
  SetAutofillWalletSpecificsFromServerProfile(address2, &profile_specifics2);
  CreditCard card2 = test::GetMaskedServerCardAmex();
  card2.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  card2.set_card_art_url(GURL("https://www.test.com/card.png"));
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);
  AutofillWalletSpecifics cloud_token_data_specifics;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, &cloud_token_data_specifics);

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
  StartSyncing({profile_specifics2, card_specifics2, customer_data_specifics,
                cloud_token_data_specifics});

  // This bridge does not store metadata, i.e. billing_address_id. Strip it
  // off so that the expectations below pass.
  card_specifics2.mutable_masked_card()->set_billing_address_id(std::string());

  // Only the server card should be present on the client.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(profile_specifics2),
                           EqualsSpecifics(card_specifics2),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics)));
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
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  AutofillWalletSpecifics cloud_token_data_specifics;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, &cloud_token_data_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics,
                cloud_token_data_specifics});

  ExpectCountsOfWalletMetadataInDB(/*cards_count=*/0u, /*address_count=*/0u);

  // This bridge does not store metadata, i.e. billing_address_id. Strip it
  // off so that the expectations below pass.
  card_specifics.mutable_masked_card()->set_billing_address_id(std::string());

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                           EqualsSpecifics(card_specifics),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics)));
}

// Tests that when a new payments customer data is sent by the server, the
// client only keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NewPaymentsCustomerData) {
  // Create one profile, one card, one customer data and one cloud token data
  // entry on the client.
  AutofillProfile address = test::GetServerProfile();
  table()->SetServerProfiles({address});
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data1{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data1);
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  // Create a different customer data entry on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(address, &profile_specifics);
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
  PaymentsCustomerData customer_data2{/*customer_id=*/kCustomerDataId2};
  AutofillWalletSpecifics customer_data_specifics2;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data2,
                                                     &customer_data_specifics2);
  AutofillWalletSpecifics cloud_token_data_specifics;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, &cloud_token_data_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics2,
                cloud_token_data_specifics});

  // Only the server card should be present on the client.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                           EqualsSpecifics(card_specifics),
                           EqualsSpecifics(customer_data_specifics2),
                           EqualsSpecifics(cloud_token_data_specifics)));
}

// Tests that when a new credit card cloud token data is sent by the server,
// the client only keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NewCloudTokenData) {
  AutofillProfile address = test::GetServerProfile();
  table()->SetServerProfiles({address});
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);
  CreditCardCloudTokenData cloud_token_data1 =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data1});

  // Create a different cloud token data entry on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(address, &profile_specifics);
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);
  CreditCardCloudTokenData cloud_token_data2 =
      test::GetCreditCardCloudTokenData2();
  AutofillWalletSpecifics cloud_token_data_specifics2;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data2, &cloud_token_data_specifics2);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics,
                cloud_token_data_specifics2});

  // Only the new cloud token data should be present on the client.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                           EqualsSpecifics(card_specifics),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics2)));
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

// Tests that when the server sends no cloud token data, the client should
// delete all it's existing cloud token data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NoCloudTokenData) {
  // Create one cloud token data on the client.
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);
  StartSyncing({});

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Tests that when the server sends the same data as the client has, nothing
// changes on the client.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeSyncData_SameWalletAddressAndCardAndCustomerDataAndCloudTokenData) {
  // Create one profile and one card on the client.
  AutofillProfile profile = test::GetServerProfile();
  table()->SetServerProfiles({profile});
  CreditCard card = test::GetMaskedServerCard();
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNENROLLED);
  card.set_card_art_url(GURL("https://www.example.com/card.png"));
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  // Create the same profile and card on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(profile, &profile_specifics);
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);
  AutofillWalletSpecifics cloud_token_data_specifics;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, &cloud_token_data_specifics);

  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  // We still need to commit the updated progress marker on the client.
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);
  StartSyncing({profile_specifics, card_specifics, customer_data_specifics,
                cloud_token_data_specifics});

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                           EqualsSpecifics(card_specifics),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics)));
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

// Test that all field values for a card sent from the server are copied on the
// card on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_SetsAllWalletCardData) {
  // Create a card to be synced from the server.
  CreditCard card = test::GetMaskedServerCard();
  card.SetNickname(u"Grocery card");
  // Set the card issuer to Google.
  card.set_card_issuer(CreditCard::Issuer::GOOGLE);
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNENROLLED);
  card.set_card_art_url(GURL("https://www.example.com/card.png"));
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
  EXPECT_EQ(card.nickname(), cards[0]->nickname());
  EXPECT_EQ(card.card_issuer(), cards[0]->card_issuer());
  EXPECT_EQ(card.instrument_id(), cards[0]->instrument_id());
  EXPECT_EQ(card.virtual_card_enrollment_state(),
            cards[0]->virtual_card_enrollment_state());
  EXPECT_EQ(card.card_art_url(), cards[0]->card_art_url());

  // Also make sure that those types are not empty, to exercice all the code
  // paths.
  EXPECT_FALSE(card.network().empty());
  EXPECT_FALSE(card.LastFourDigits().empty());
  EXPECT_NE(0, card.expiration_month());
  EXPECT_NE(0, card.expiration_year());
  EXPECT_FALSE(card.nickname().empty());
  EXPECT_NE(0, card.instrument_id());
}

// Test that all field values for a cloud token data sent from the server are
// copied on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_SetsAllCloudTokenData) {
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  AutofillWalletSpecifics cloud_token_data_specifics;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, &cloud_token_data_specifics);

  StartSyncing({cloud_token_data_specifics});

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(cloud_token_data_specifics)));
  std::vector<std::unique_ptr<CreditCardCloudTokenData>>
      cloud_token_data_vector;
  table()->GetCreditCardCloudTokenData(&cloud_token_data_vector);
  ASSERT_EQ(1U, cloud_token_data_vector.size());

  EXPECT_EQ(cloud_token_data.masked_card_id,
            cloud_token_data_vector[0]->masked_card_id);
  EXPECT_EQ(cloud_token_data.suffix, cloud_token_data_vector[0]->suffix);
  EXPECT_EQ(cloud_token_data.exp_month, cloud_token_data_vector[0]->exp_month);
  EXPECT_EQ(cloud_token_data.exp_year, cloud_token_data_vector[0]->exp_year);
  EXPECT_EQ(cloud_token_data.card_art_url,
            cloud_token_data_vector[0]->card_art_url);
  EXPECT_EQ(cloud_token_data.instrument_token,
            cloud_token_data_vector[0]->instrument_token);
}

TEST_F(AutofillWalletSyncBridgeTest, LoadMetadataCalled) {
  EXPECT_TRUE(table()->UpdateEntityMetadata(syncer::AUTOFILL_WALLET_DATA, "key",
                                            EntityMetadata()));

  ResetProcessor();
  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/HasInitialSyncDone(),
                                    /*entities=*/SizeIs(1))));
  ResetBridge(/*initial_sync_done=*/true);
}

TEST_F(AutofillWalletSyncBridgeTest, ApplyStopSyncChanges_ClearAllData) {
  // Create one profile, one card and one cloud token data on the client.
  AutofillProfile local_profile = test::GetServerProfile();
  table()->SetServerProfiles({local_profile});
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);

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
  // Create one profile, one card and one cloud token data on the client.
  AutofillProfile local_profile = test::GetServerProfile();
  table()->SetServerProfiles({local_profile});
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  // We do not write to DB at all, so we should not commit any changes.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfMultipleAutofillChanges()).Times(0);
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);

  // Passing in a non-null metadata change list indicates to the bridge that
  // sync is stopping but the data type is not disabled.
  bridge()->ApplyStopSyncChanges(/*delete_metadata_change_list=*/nullptr);

  EXPECT_FALSE(GetAllLocalData().empty());
}

// This test ensures that an int64 -> int conversion bug we encountered is
// fixed.
TEST_F(AutofillWalletSyncBridgeTest,
       LargeInstrumentIdProvided_CorrectDataStored) {
  // Create a card to be synced from the server.
  CreditCard card = test::GetMaskedServerCard();
  // Set instrument_id to be the largest int64_t.
  card.set_instrument_id(INT64_MAX);
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);

  StartSyncing({card_specifics});

  std::vector<std::unique_ptr<CreditCard>> cards;
  table()->GetServerCreditCards(&cards);
  ASSERT_EQ(1U, cards.size());

  // Make sure that the correct instrument_id was set.
  EXPECT_EQ(card.instrument_id(), cards[0]->instrument_id());
  EXPECT_EQ(INT64_MAX, cards[0]->instrument_id());
}

// Test that it logs correctly when new cards with virtual card metadata are
// synced.
TEST_F(AutofillWalletSyncBridgeTest, SetWalletCards_LogVirtualMetadataSynced) {
  // Initial data:
  // Card 1: has virtual cards.
  CreditCard card1 = test::GetMaskedServerCard();
  card1.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  card1.set_server_id("card1_server_id");
  card1.set_card_art_url(GURL("https://www.example.com/card1.png"));
  // Card 2: has virtual cards.
  CreditCard card2 = test::GetMaskedServerCard();
  card2.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  card2.set_server_id("card2_server_id");
  card2.set_card_art_url(GURL("https://www.example.com/card2.png"));
  // Card 3: has no virtual cards
  CreditCard card3 = test::GetMaskedServerCard();
  card3.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNENROLLED);
  card3.set_server_id("card3_server_id");

  table()->SetServerCreditCards({card1, card2, card3});

  // Trigger sync:
  // Card 1: Same as old card 1. No data changed; should not log.
  AutofillWalletSpecifics card1_specifics;
  SetAutofillWalletSpecificsFromServerCard(card1, &card1_specifics);
  // Card 2: Updated the card art url; should log for existing card.
  AutofillWalletSpecifics card2_specifics;
  card2.set_card_art_url(GURL("https://www.example.com/card2-new.png"));
  SetAutofillWalletSpecificsFromServerCard(card2, &card2_specifics);
  // Card 3: Existed card newly-enrolled in virtual cards; should log for new
  // card.
  AutofillWalletSpecifics card3_specifics;
  card3.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  card3.set_card_art_url(GURL("https://www.example.com/card3.png"));
  SetAutofillWalletSpecificsFromServerCard(card3, &card3_specifics);
  // Card 4: New card enrolled in virtual cards; should log for new card
  AutofillWalletSpecifics card4_specifics;
  CreditCard card4 = test::GetMaskedServerCard();
  card4.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  card4.set_server_id("card4_server_id");
  card4.set_card_art_url(GURL("https://www.example.com/card4.png"));
  SetAutofillWalletSpecificsFromServerCard(card4, &card4_specifics);

  // This bridge does not store metadata, i.e. billing_address_id. Strip it
  // off so that the expectations below pass.
  card1_specifics.mutable_masked_card()->set_billing_address_id(std::string());
  card2_specifics.mutable_masked_card()->set_billing_address_id(std::string());
  card3_specifics.mutable_masked_card()->set_billing_address_id(std::string());
  card4_specifics.mutable_masked_card()->set_billing_address_id(std::string());

  std::vector<std::string> server_ids = {"card2_server_id", "card3_server_id",
                                         "card4_server_id"};

  // Trigger sync.
  base::HistogramTester histogram_tester;
  StartSyncing(
      {card1_specifics, card2_specifics, card3_specifics, card4_specifics});

  // Verify the histogram logs.
  histogram_tester.ExpectBucketCount("Autofill.VirtualCard.MetadataSynced",
                                     /*existing_card*/ false, 1);
  histogram_tester.ExpectBucketCount("Autofill.VirtualCard.MetadataSynced",
                                     /*existing_card*/ true, 2);
}

}  // namespace autofill
