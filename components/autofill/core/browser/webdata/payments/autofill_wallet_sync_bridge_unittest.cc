// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/autofill_wallet_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit_test_api.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/payments_metadata.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using autofill::AutofillProfileChange;
using autofill::CreditCardChange;
using base::ScopedTempDir;
using IbanChangeKey = absl::variant<std::string, int64_t>;
using sync_pb::AutofillWalletSpecifics;
using sync_pb::DataTypeState;
using sync_pb::EntityMetadata;
using syncer::DataBatch;
using syncer::DataType;
using syncer::EntityChange;
using syncer::EntityData;
using syncer::HasInitialSyncDone;
using syncer::KeyAndData;
using syncer::MockDataTypeLocalChangeProcessor;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

// Represents a Payments customer id.
const char kCustomerDataId[] = "deadbeef";
const char kCustomerDataId2[] = "deadcafe";

// Unique client tags for the server data.
const char kCard1ClientTag[] = "Y2FyZDHvv74=";
const char kCustomerDataClientTag[] = "deadbeef";
const char kCloudTokenDataClientTag[] = "token";
const char kBankAccountClientTag[] = "payment_instrument:1234";
const char kEwalletAccountClientTag[] = "payment_instrument:2345";

const base::Time kJune2017 = base::Time::FromSecondsSinceUnixEpoch(1497552271);

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
         << ", product_terms_url: "
         << specifics.masked_card().product_terms_url() << ", card_benefits: {";

  for (auto benefit : specifics.masked_card().card_benefit()) {
    output << "[benefit_id: " << benefit.benefit_id()
           << ", benefit_description: " << benefit.benefit_description();
    if (benefit.has_start_time_unix_epoch_milliseconds()) {
      output << ", start_time: ", benefit.start_time_unix_epoch_milliseconds();
    }
    if (benefit.has_end_time_unix_epoch_milliseconds()) {
      output << ", end_time: ", benefit.end_time_unix_epoch_milliseconds();
    }
    if (benefit.has_flat_rate_benefit()) {
      output << ", benefit_type: flat_rate_benefit";
    } else if (benefit.has_category_benefit()) {
      output << ", benefit_type: category_benefit, benefit_category: "
             << benefit.category_benefit().category_benefit_type();
    } else if (benefit.has_merchant_benefit()) {
      output << ", benefit_type: merchant_benefit, merchant_domains: {";
      for (std::string merchant_domain :
           benefit.merchant_benefit().merchant_domain()) {
        output << merchant_domain << ", ";
      }
      output << "}";
    }
    output << "], ";
  }

  output << "}]";
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

std::string WalletBankAccountDetailsAsDebugString(
    const sync_pb::BankAccountDetails& bank_account_details) {
  std::ostringstream output;
  output << "[bank_name: " << bank_account_details.bank_name()
         << "account_number_suffix: "
         << bank_account_details.account_number_suffix()
         << "account_type: " << bank_account_details.account_type() << "]";
  return output.str();
}

std::string WalletPaymentInstrumentSupportedRailAsDebugString(
    const sync_pb::PaymentInstrument payment_instrument) {
  std::vector<std::string_view> supported_rails;
  for (int supported_rail : payment_instrument.supported_rails()) {
    supported_rails.push_back(
        sync_pb::PaymentInstrument::SupportedRail_Name(supported_rail));
  }
  return base::JoinString(supported_rails, ",");
}

std::string WalletPaymentInstrumentAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  std::ostringstream output;
  output << "[instrument_id: " << specifics.payment_instrument().instrument_id()
         << "supported_rails: "
         << WalletPaymentInstrumentSupportedRailAsDebugString(
                specifics.payment_instrument())
         << "display_icon_url: "
         << specifics.payment_instrument().display_icon_url()
         << "nickname: " << specifics.payment_instrument().nickname();
  if (specifics.payment_instrument().instrument_details_case() ==
      sync_pb::PaymentInstrument::InstrumentDetailsCase::kBankAccount) {
    output << WalletBankAccountDetailsAsDebugString(
        specifics.payment_instrument().bank_account());
  }
  output << "]";
  return output.str();
}

std::string WalletMaskedIbanSpecificsAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  std::ostringstream output;
  output << "[id: " << specifics.masked_iban().instrument_id()
         << ", prefix: " << specifics.masked_iban().prefix()
         << ", suffix: " << specifics.masked_iban().suffix()
         << ", length: " << specifics.masked_iban().length()
         << ", nickname: " << specifics.masked_iban().nickname() << "]";
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
      return "POSTAL_ADDRESS is deprecated";
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_CUSTOMER_DATA:
      return "CustomerData";
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_CREDIT_CARD_CLOUD_TOKEN_DATA:
      return WalletCreditCardCloudTokenDataSpecificsAsDebugString(specifics);
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_PAYMENT_INSTRUMENT:
      return WalletPaymentInstrumentAsDebugString(specifics);
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_MASKED_IBAN:
      return WalletMaskedIbanSpecificsAsDebugString(specifics);
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
  if (arg.type() != decltype(arg.type())::REMOVE) {
    *result_listener << "type " << arg.type() << " is not REMOVE";
    return false;
  }
  if (arg.key() != key) {
    *result_listener << "keys don't match";
  }
  return true;
}

MATCHER_P2(AddChange, key, data, "") {
  if (arg.type() != decltype(arg.type())::ADD) {
    return false;
  }
  if (arg.key() != key) {
    *result_listener << "keys don't match";
  }
  if (arg.data_model() != data) {
    *result_listener << "data " << arg.data_model()
                     << " does not match expected " << data;
  }
  return true;
}

class AutofillWalletSyncBridgeTestBase {
 public:
  AutofillWalletSyncBridgeTestBase()
      : encryptor_(os_crypt_async::GetTestEncryptorForTesting()) {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&sync_metadata_table_);
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"),
             &encryptor_);
    ON_CALL(*backend(), GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
    // Fake that initial sync has been done (so that the bridge immediately
    // records metrics).
    ResetBridge(/*initial_sync_done=*/true);
  }

  AutofillWalletSyncBridgeTestBase(const AutofillWalletSyncBridgeTestBase&) =
      delete;
  AutofillWalletSyncBridgeTestBase& operator=(
      const AutofillWalletSyncBridgeTestBase&) = delete;

  ~AutofillWalletSyncBridgeTestBase() = default;

  void ResetProcessor() {
    real_processor_ = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
        syncer::AUTOFILL_WALLET_DATA, /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge(bool initial_sync_done) {
    DataTypeState data_type_state;
    data_type_state.set_initial_sync_state(
        initial_sync_done
            ? sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE
            : sync_pb::
                  DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
    data_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(syncer::AUTOFILL_WALLET_DATA));
    data_type_state.set_cache_guid(kDefaultCacheGuid);
    EXPECT_TRUE(sync_metadata_table()->UpdateDataTypeState(
        syncer::AUTOFILL_WALLET_DATA, data_type_state));
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

    // ClientTagBasedDataTypeProcessor requires connecting before other
    // interactions with the worker happen.
    real_processor_->ConnectSync(
        std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());

    // Initialize the processor with the initial sync already done.
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

    sync_pb::GarbageCollectionDirective gc_directive;
    gc_directive.set_version_watermark(1);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillWalletSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates),
                                      gc_directive);
  }

  void MergeFullSyncDataForCards(
      const std::vector<AutofillWalletSpecifics>& remote_data = {}) {
    syncer::EntityChangeList entity_data;
    for (const AutofillWalletSpecifics& specifics : remote_data) {
      entity_data.push_back(syncer::EntityChange::CreateAdd(
          specifics.masked_card().id(), SpecificsToEntity(specifics)));
    }
    bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                                std::move(entity_data));
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
    ExtractAutofillWalletSpecificsFromDataBatch(
        bridge()->GetAllDataForTesting(), &data);
    return data;
  }

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const AutofillWalletSpecifics& specifics) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics);
    return data;
  }

  // Helper to link given `card` with `benefit`.
  void LinkCardAndBenefit(CreditCard& card, CreditCardBenefit& benefit) {
    card.set_product_terms_url(GURL("https://www.example.com/term"));
    test_api(benefit).SetLinkedCardInstrumentId(
        CreditCardBenefitBase::LinkedCardInstrumentId(card.instrument_id()));
  }

  // Helper to get the specifics for given `card` and `benefit`.
  // Returns the card specifics for future testing.
  AutofillWalletSpecifics GetSpecificsFromCardAndBenefit(
      const CreditCard& card,
      const CreditCardBenefit& benefit) {
    // Billing address IDs are deprecated and no longer stored.
    EXPECT_TRUE(card.billing_address_id().empty());

    AutofillWalletSpecifics card_specifics;
    SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
    SetAutofillWalletSpecificsFromCardBenefit(benefit, /*enforce_utf8=*/false,
                                              card_specifics);

    return card_specifics;
  }

  AutofillWalletSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  PaymentsAutofillTable* table() { return &table_; }
  AutofillSyncMetadataTable* sync_metadata_table() {
    return &sync_metadata_table_;
  }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  const os_crypt_async::Encryptor encryptor_;
  NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillSyncMetadataTable sync_metadata_table_;
  PaymentsAutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedDataTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletSyncBridge> bridge_;
};

class AutofillWalletSyncBridgeTest : public testing::Test,
                                     public AutofillWalletSyncBridgeTestBase {
 public:
  AutofillWalletSyncBridgeTest() {
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableCardBenefitsSync);
  }

  ~AutofillWalletSyncBridgeTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// The following 3 tests make sure client tags stay stable.
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

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForBankAccount) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForBankAccount(
          kBankAccountClientTag,
          /*nickname=*/"Pix bank account",
          /*display_icon_url=*/GURL("http://www.google.com"),
          /*bank_name=*/"ABC Bank",
          /*account_number_suffix=*/"1234",
          sync_pb::BankAccountDetails_AccountType_CHECKING);

  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kBankAccountClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForEwalletAccount) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForEwalletAccount(
          /*client_tag=*/kEwalletAccountClientTag,
          /*nickname=*/"eWallet account",
          /*display_icon_url=*/GURL("http://www.google.com"),
          /*ewallet_name=*/"ABC Pay",
          /*account_display_name=*/"2345",
          /*is_fido_enrolled=*/false);

  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            kEwalletAccountClientTag);
}

// The following 3 tests make sure storage keys stay stable.
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

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForBankAccount) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForBankAccount(
          kBankAccountClientTag,
          /*nickname=*/"Pix bank account",
          /*display_icon_url=*/GURL("http://www.google.com"),
          /*bank_name=*/"ABC Bank",
          /*account_number_suffix=*/"1234",
          sync_pb::BankAccountDetails_AccountType_CHECKING);

  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            kBankAccountClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForEwalletAccount) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForEwalletAccount(
          /*client_tag=*/kEwalletAccountClientTag,
          /*nickname=*/"eWallet account",
          /*display_icon_url=*/GURL("http://www.google.com"),
          /*ewallet_name=*/"ABC Pay",
          /*account_display_name=*/"5678",
          /*is_fido_enrolled=*/false);

  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics)),
            kEwalletAccountClientTag);
}

TEST_F(AutofillWalletSyncBridgeTest,
       GetAllDataForDebugging_ShouldReturnAllData) {
  // Create Wallet Data and store them in the table.
  CreditCard card1 = test::GetMaskedServerCard();
  // Set the card issuer to Google.
  card1.set_card_issuer(CreditCard::Issuer::kGoogle);
  card1.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolled);
  card1.set_card_art_url(GURL("https://www.example.com/card.png"));
  CreditCard card2 = test::GetMaskedServerCardAmex();
  CreditCard card_with_nickname = test::GetMaskedServerCardWithNickname();
  card2.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  card2.set_virtual_card_enrollment_type(
      CreditCard::VirtualCardEnrollmentType::kNetwork);
  table()->SetServerCreditCards({card1, card2, card_with_nickname});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);
  CreditCardCloudTokenData data1 = test::GetCreditCardCloudTokenData1();
  CreditCardCloudTokenData data2 = test::GetCreditCardCloudTokenData2();
  table()->SetCreditCardCloudTokenData({data1, data2});

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
  EXPECT_EQ(sync_pb::WalletMaskedCreditCard::NETWORK,
            card_specifics2.masked_card().virtual_card_enrollment_type());
  EXPECT_EQ("https://www.example.com/card.png",
            card_specifics1.masked_card().card_art_url());
  EXPECT_TRUE(card_specifics2.masked_card().card_art_url().empty());

  // Read local Wallet Data from Autofill table, and compare with expected
  // wallet specifics.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(card_specifics1),
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

// Tests that when a new wallet card is sent by the server, the client only
// keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NewWalletCard) {
  // Create one card on the client.
  CreditCard card1 = test::GetMaskedServerCard();
  card1.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolled);
  card1.set_card_art_url(GURL("https://www.example.com/card.png"));
  table()->SetServerCreditCards({card1});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  // Create a different card on the server.
  CreditCard card2 = test::GetMaskedServerCardAmex();
  card2.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  card2.set_card_art_url(GURL("https://www.test.com/card.png"));
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);
  AutofillWalletSpecifics cloud_token_data_specifics;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, &cloud_token_data_specifics);

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card2.server_id(), card2)));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(card1.server_id())));
  StartSyncing(
      {card_specifics2, customer_data_specifics, cloud_token_data_specifics});

  // Billing address IDs are deprecated and no longer stored.
  card_specifics2.mutable_masked_card()->set_billing_address_id(std::string());

  // Only the server card should be present on the client.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(card_specifics2),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics)));
}

// Tests that in initial sync, no metrics are recorded for new cards.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_NewWalletCardNoMetricsInitialSync) {
  ResetProcessor();
  ResetBridge(/*initial_sync_done=*/false);

  // Create a data set on the server.
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

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing(
      {card_specifics, customer_data_specifics, cloud_token_data_specifics});

  std::vector<PaymentsMetadata> cards_metadata;
  ASSERT_TRUE(table()->GetServerCardsMetadata(cards_metadata));
  EXPECT_EQ(0u, cards_metadata.size());

  // Billing address IDs are deprecated and no longer stored.
  card_specifics.mutable_masked_card()->set_billing_address_id(std::string());

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(card_specifics),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics)));
}

// Tests that in initial sync, no metadata are synced for new IBANs.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_NewWalletIbanNoMetadataInitialSync) {
  ResetProcessor();
  ResetBridge(/*initial_sync_done=*/false);

  // Create a data set on the server.
  Iban iban = test::GetServerIban();
  AutofillWalletSpecifics iban_specifics;
  SetAutofillWalletSpecificsFromMaskedIban(iban, &iban_specifics);

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({iban_specifics});

  std::vector<PaymentsMetadata> ibans_metadata;
  ASSERT_TRUE(table()->GetServerIbansMetadata(ibans_metadata));
  EXPECT_EQ(0u, ibans_metadata.size());

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(iban_specifics)));
}

// Tests that when a new payments customer data is sent by the server, the
// client only keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_NewPaymentsCustomerData) {
  // Create one card, one customer data and one cloud token data entry on the
  // client.
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data1{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data1);
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  // Create a different customer data entry on the server.
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
  PaymentsCustomerData customer_data2{/*customer_id=*/kCustomerDataId2};
  AutofillWalletSpecifics customer_data_specifics2;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data2,
                                                     &customer_data_specifics2);
  AutofillWalletSpecifics cloud_token_data_specifics;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, &cloud_token_data_specifics);

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfAutofillProfileChanged).Times(0);
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);
  StartSyncing(
      {card_specifics, customer_data_specifics2, cloud_token_data_specifics});

  // Only the server card should be present on the client.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(card_specifics),
                           EqualsSpecifics(customer_data_specifics2),
                           EqualsSpecifics(cloud_token_data_specifics)));
}

// Tests that when a new credit card cloud token data is sent by the server,
// the client only keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NewCloudTokenData) {
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);
  CreditCardCloudTokenData cloud_token_data1 =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data1});

  // Create a different cloud token data entry on the server.
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

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);
  StartSyncing(
      {card_specifics, customer_data_specifics, cloud_token_data_specifics2});

  // Only the new cloud token data should be present on the client.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(card_specifics),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics2)));
}

// Tests that when the server sends no cards, the client should delete all it's
// existing data.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NoWalletCard) {
  // Create one card on the client.
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(local_card.server_id())));
  StartSyncing({});

  // This bridge should not touch the metadata; should get deleted by the
  // metadata bridge.
  std::vector<PaymentsMetadata> cards_metadata;
  ASSERT_TRUE(table()->GetServerCardsMetadata(cards_metadata));
  EXPECT_EQ(1u, cards_metadata.size());

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Tests that when the server sends no IBANs, the client should delete all it's
// existing data.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NoWalletIban) {
  // Create one IBAN on the client.
  Iban existing_iban = test::GetServerIban();
  table()->SetServerIbansForTesting({existing_iban});

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfIbanChanged(RemoveChange(
                              IbanChangeKey(existing_iban.instrument_id()))));
  StartSyncing({});

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Tests that when the server sends no cloud token data, the client should
// delete all it's existing cloud token data.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NoCloudTokenData) {
  // Create one cloud token data on the client.
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);
  StartSyncing({});

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Tests that when the server sends the same data as the client has, nothing
// changes on the client.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_SameWalletCardAndCustomerDataAndCloudTokenData) {
  // Create one card on the client.
  CreditCard card = test::GetMaskedServerCard();
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolled);
  card.set_card_art_url(GURL("https://www.example.com/card.png"));
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  // Create the card on the server.
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);
  AutofillWalletSpecifics cloud_token_data_specifics;
  SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
      cloud_token_data, &cloud_token_data_specifics);

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA))
      .Times(0);
  // We still need to commit the updated progress marker on the client.
  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);
  StartSyncing(
      {card_specifics, customer_data_specifics, cloud_token_data_specifics});

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(card_specifics),
                           EqualsSpecifics(customer_data_specifics),
                           EqualsSpecifics(cloud_token_data_specifics)));
}

// Tests that when there are multiple changes happening at the same time, the
// data from the server is what the client ends up with.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_AddRemoveAndPreserveWalletCard) {
  // Create one card on the client.
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});
  PaymentsCustomerData customer_data{/*customer_id=*/kCustomerDataId};
  table()->SetPaymentsCustomerData(&customer_data);

  // Create a different card on the server.
  // The Amex card has different values for the relevant fields.
  CreditCard card2 = test::GetMaskedServerCardAmex();
  AutofillWalletSpecifics card2_specifics;
  SetAutofillWalletSpecificsFromServerCard(card2, &card2_specifics);
  AutofillWalletSpecifics customer_data_specifics;
  SetAutofillWalletSpecificsFromPaymentsCustomerData(customer_data,
                                                     &customer_data_specifics);

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(RemoveChange(card.server_id())));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card2.server_id(), card2)));
  StartSyncing({card2_specifics, customer_data_specifics});

  // Billing address IDs are deprecated and no longer stored.
  card2_specifics.mutable_masked_card()->set_billing_address_id(std::string());

  // Make sure that the client only has the data from the server.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card2_specifics),
                                   EqualsSpecifics(customer_data_specifics)));
}

// Test that all field values for a card sent from the server are copied on the
// card on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_SetsAllWalletCardData) {
  // Create a card to be synced from the server.
  CreditCard card = test::GetMaskedServerCard();
  card.SetNickname(u"Grocery card");
  // Set the card issuer to Google.
  card.set_card_issuer(CreditCard::Issuer::kGoogle);
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolled);
  card.set_card_art_url(GURL("https://www.example.com/card.png"));
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);

  StartSyncing({card_specifics});

  // Billing address IDs are deprecated and no longer stored.
  card.set_billing_address_id(std::string());
  card_specifics.mutable_masked_card()->set_billing_address_id(std::string());

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));

  std::vector<std::unique_ptr<CreditCard>> cards;
  table()->GetServerCreditCards(cards);
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
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_SetsAllCloudTokenData) {
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
  table()->GetCreditCardCloudTokenData(cloud_token_data_vector);
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

// Tests that all field values for an IBAN sent from the server are copied on
// the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_SetsNewMaskedIban) {
  Iban server_iban = test::GetServerIban();

  AutofillWalletSpecifics masked_iban_specifics;
  SetAutofillWalletSpecificsFromMaskedIban(server_iban, &masked_iban_specifics);

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(),
              NotifyOfIbanChanged(AddChange(
                  IbanChangeKey(server_iban.instrument_id()), server_iban)));
  StartSyncing({masked_iban_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(masked_iban_specifics)));
  std::vector<std::unique_ptr<Iban>> iban_vector;
  ASSERT_TRUE(table()->GetServerIbans(iban_vector));
  EXPECT_THAT(iban_vector, UnorderedElementsAre(testing::Pointee(server_iban)));
}

// Tests that when there are existing IBANs, the data from the server is what
// the client ends up with.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_UpdateAndRemoveMaskedIban) {
  Iban server_iban1 = test::GetServerIban();
  table()->SetServerIbansForTesting({server_iban1});

  // Create a `server_iban2` which has the same data as `server_iban1` but with
  // a different nickname.
  Iban server_iban2 = server_iban1;
  server_iban2.set_nickname(u"Another nickname");
  AutofillWalletSpecifics masked_iban2_specifics;
  SetAutofillWalletSpecificsFromMaskedIban(server_iban2,
                                           &masked_iban2_specifics);

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), NotifyOfIbanChanged(RemoveChange(
                              IbanChangeKey(server_iban1.instrument_id()))));
  EXPECT_CALL(*backend(),
              NotifyOfIbanChanged(AddChange(
                  IbanChangeKey(server_iban2.instrument_id()), server_iban2)));
  StartSyncing({masked_iban2_specifics});

  std::vector<std::unique_ptr<Iban>> iban_vector;
  ASSERT_TRUE(table()->GetServerIbans(iban_vector));
  EXPECT_THAT(iban_vector,
              UnorderedElementsAre(testing::Pointee(server_iban2)));
}

// Test to verify the deletion of the server cvc for the card with the `REMOVE`
// change tag.
TEST_F(AutofillWalletSyncBridgeTest, DeletesServerCvcWhenCardIsDeleted) {
  // Add a masked card and its CVC. Then also add an orphaned CVC.
  CreditCard credit_card = test::GetMaskedServerCard();
  const ServerCvc server_cvc =
      ServerCvc{credit_card.instrument_id(), u"123",
                base::Time::UnixEpoch() + base::Milliseconds(25000)};
  const ServerCvc server_cvc_2 =
      ServerCvc{test::GetMaskedServerCard2().instrument_id(), u"890",
                base::Time::UnixEpoch() + base::Milliseconds(50000)};

  table()->AddServerCvc(server_cvc);
  table()->AddServerCvc(server_cvc_2);
  ASSERT_EQ(table()->GetAllServerCvcs().size(), 2u);

  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(credit_card, &card_specifics);
  // Sync the server cards.
  // This should cause the orphaned CVC to get deleted.
  StartSyncing({card_specifics});

  ASSERT_EQ(table()->GetAllServerCvcs().size(), 1u);
  EXPECT_EQ(*(table()->GetAllServerCvcs()[0]), server_cvc);
}

TEST_F(AutofillWalletSyncBridgeTest, LoadMetadataCalled) {
  EXPECT_TRUE(sync_metadata_table()->UpdateEntityMetadata(
      syncer::AUTOFILL_WALLET_DATA, "key", EntityMetadata()));

  ResetProcessor();
  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/HasInitialSyncDone(),
                                    /*entities=*/SizeIs(1))));
  ResetBridge(/*initial_sync_done=*/true);
}

TEST_F(AutofillWalletSyncBridgeTest, ApplyDisableSyncChanges_Cards) {
  // Create one card and one cloud token data on the client.
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});
  CreditCardCloudTokenData cloud_token_data =
      test::GetCreditCardCloudTokenData1();
  table()->SetCreditCardCloudTokenData({cloud_token_data});

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), NotifyOfCreditCardChanged).Times(0);

  // ApplyDisableSyncChanges indicates to the bridge that sync is stopping
  // because it was disabled.
  bridge()->ApplyDisableSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>());

  // This bridge should not touch the metadata; should get deleted by the
  // metadata bridge.
  std::vector<PaymentsMetadata> cards_metadata;
  ASSERT_TRUE(table()->GetServerCardsMetadata(cards_metadata));
  EXPECT_EQ(1u, cards_metadata.size());

  EXPECT_TRUE(GetAllLocalData().empty());
}

TEST_F(AutofillWalletSyncBridgeTest, ApplyDisableSyncChanges_Ibans) {
  // Create one IBAN on the client.
  Iban existing_iban = test::GetServerIban();
  table()->SetServerIbansForTesting({existing_iban});

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), NotifyOfIbanChanged).Times(0);

  // ApplyDisableSyncChanges indicates to the bridge that sync is stopping
  // because it was disabled.
  bridge()->ApplyDisableSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>());

  EXPECT_TRUE(GetAllLocalData().empty());
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
  table()->GetServerCreditCards(cards);
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
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  card1.set_server_id("card1_server_id");
  card1.set_card_art_url(GURL("https://www.example.com/card1.png"));
  // Card 2: has virtual cards.
  CreditCard card2 = test::GetMaskedServerCard();
  card2.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  card2.set_server_id("card2_server_id");
  card2.set_card_art_url(GURL("https://www.example.com/card2.png"));
  // Card 3: has no virtual cards
  CreditCard card3 = test::GetMaskedServerCard();
  card3.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolled);
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
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  card3.set_card_art_url(GURL("https://www.example.com/card3.png"));
  SetAutofillWalletSpecificsFromServerCard(card3, &card3_specifics);
  // Card 4: New card enrolled in virtual cards; should log for new card
  AutofillWalletSpecifics card4_specifics;
  CreditCard card4 = test::GetMaskedServerCard();
  card4.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  card4.set_server_id("card4_server_id");
  card4.set_card_art_url(GURL("https://www.example.com/card4.png"));
  SetAutofillWalletSpecificsFromServerCard(card4, &card4_specifics);

  // Billing address IDs are deprecated and no longer stored.
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

// Tests that all field values for `CreditCardBenefits` sent from the server
// are copied to the client when the `product_terms_url` is not empty.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_SetsNewCardBenefitWithTerms) {
  // Get a card and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  StartSyncing({card_specifics});

  // The client data should match the server specifics.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
  std::vector<std::unique_ptr<CreditCard>> database_cards;
  std::vector<CreditCardBenefit> database_benefits;
  ASSERT_TRUE(table()->GetServerCreditCards(database_cards));
  ASSERT_TRUE(table()->GetAllCreditCardBenefits(database_benefits));
  ASSERT_EQ(1U, database_cards.size());
  EXPECT_THAT(database_cards[0]->product_terms_url(), card.product_terms_url());
  EXPECT_THAT(database_benefits, UnorderedElementsAre(card_benefit));
}

// Tests that all benefit related field values are deleted from client when the
// card from server has no `product_terms_url`.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_SetsNewCardBenefitWithoutTerms) {
  // Get a card with no `product_terms_url` and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);
  card.set_product_terms_url(GURL());

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  StartSyncing({card_specifics});

  // The benefit related data will be deleted from the client since there
  // is no `product_terms_url`.
  card_specifics.mutable_masked_card()->clear_card_benefit();

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
  std::vector<std::unique_ptr<CreditCard>> database_cards;
  std::vector<CreditCardBenefit> database_benefits;
  ASSERT_TRUE(table()->GetServerCreditCards(database_cards));
  ASSERT_TRUE(table()->GetAllCreditCardBenefits(database_benefits));
  ASSERT_EQ(1U, database_cards.size());
  EXPECT_TRUE(database_cards[0]->product_terms_url().is_empty());
  EXPECT_TRUE(database_benefits.empty());
}

// Tests that when a new card benefit is sent by the server, the client only
// keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NewCardBenefit) {
  // Get a card and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA))
      .Times(2);
  EXPECT_CALL(*backend(), CommitChanges()).Times(2);
  // Card should be added once as the data is not card changed in the 2nd
  // attempt.
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card.server_id(), card)));

  StartSyncing({card_specifics});
  // Update specifics with a different benefit.
  card_specifics.mutable_masked_card()->mutable_card_benefit(0)->set_benefit_id(
      "DifferentId");
  MergeFullSyncDataForCards({card_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
}

// Tests that the client should delete all its existing benefits when the
// server sends no benefits.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NoCardBenefit) {
  // Get a card and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA))
      .Times(2);
  EXPECT_CALL(*backend(), CommitChanges()).Times(2);
  // Card should only be added once, since it is not changed between the two
  // syncs.
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card.server_id(), card)));

  StartSyncing({card_specifics});
  // Delete benefit data for 2nd sync attempt.
  card_specifics.mutable_masked_card()->clear_card_benefit();
  MergeFullSyncDataForCards({card_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
}

// Tests that when the server sends the same data as the client has, nothing
// changes on the client.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_SameWalletCardAndCardBenefit) {
  // Get a card and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  // Client card and benefit should only be updated once since data remains
  // the same between the two syncs.
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card.server_id(), card)));
  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  //  We need to commit the updated progress marker on the client even with the
  //  same data.
  EXPECT_CALL(*backend(), CommitChanges()).Times(2);

  StartSyncing({card_specifics});
  MergeFullSyncDataForCards({card_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
}

// Test suite for sync bridge with the benefit syncing feature disabled.
class AutofillWalletSyncBridgeTestWithBenefitSyncDisabled
    : public testing::Test,
      public AutofillWalletSyncBridgeTestBase {
 public:
  AutofillWalletSyncBridgeTestWithBenefitSyncDisabled() {
    feature_list_.InitAndDisableFeature(
        autofill::features::kAutofillEnableCardBenefitsSync);
  }

  ~AutofillWalletSyncBridgeTestWithBenefitSyncDisabled() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that benefit related data will not be saved on client when the benefit
// syncing feature is disabled.
TEST_F(AutofillWalletSyncBridgeTestWithBenefitSyncDisabled,
       MergeFullSyncData_SetsNewCardBenefitWithTerms) {
  // Get a card and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  StartSyncing({card_specifics});

  // No benefit related data should be saved on client since the benefit
  // syncing feature is disabled.
  card_specifics.mutable_masked_card()->clear_card_benefit();
  card_specifics.mutable_masked_card()->clear_product_terms_url();

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
  std::vector<std::unique_ptr<CreditCard>> database_cards;
  std::vector<CreditCardBenefit> database_benefits;
  ASSERT_TRUE(table()->GetServerCreditCards(database_cards));
  ASSERT_TRUE(table()->GetAllCreditCardBenefits(database_benefits));
  ASSERT_EQ(1U, database_cards.size());
  EXPECT_TRUE(database_cards[0]->product_terms_url().is_empty());
  EXPECT_TRUE(database_benefits.empty());
}

// Tests that when a new card benefit is sent by the server, the client will
// delete all benefit related data if the benefit syncing feature is disabled.
TEST_F(AutofillWalletSyncBridgeTestWithBenefitSyncDisabled,
       MergeFullSyncData_NewCardBenefit) {
  // Get a card and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  // Card and benefit should remain the same between the two sync attempt,
  // since the benefit is not saved.
  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card.server_id(), card)));
  //  We need to commit the updated progress marker on the client even with the
  //  same data.
  EXPECT_CALL(*backend(), CommitChanges()).Times(2);

  StartSyncing({card_specifics});
  // Update specifics with a different benefit.
  card_specifics.mutable_masked_card()->mutable_card_benefit(0)->set_benefit_id(
      "DifferentId");
  MergeFullSyncDataForCards({card_specifics});

  // `product_terms_url` of the client card and client benfit should be deleted.
  card_specifics.mutable_masked_card()->clear_product_terms_url();
  card_specifics.mutable_masked_card()->clear_card_benefit();

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
}

// Tests that the client should delete all benefit related data when
// the benefit syncing feature is disabled.
TEST_F(AutofillWalletSyncBridgeTestWithBenefitSyncDisabled,
       MergeFullSyncData_NoCardBenefit) {
  // Get a card and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  // Card and benefit should remain the same between the two syncs.
  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card.server_id(), card)));
  //  We need to commit the updated progress marker on the client even with the
  //  same data.
  EXPECT_CALL(*backend(), CommitChanges()).Times(2);

  StartSyncing({card_specifics});
  // Delete benefit data for the 2nd sync attempt.
  card_specifics.mutable_masked_card()->clear_card_benefit();
  MergeFullSyncDataForCards({card_specifics});

  // `product_terms_url` should be deleted when the sync is disabled.
  card_specifics.mutable_masked_card()->clear_product_terms_url();

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
}

// Tests that when the server sends the same data as the client has, benefit
// related data should be deleted from client when the benefit syncing feature
// is disabled.
TEST_F(AutofillWalletSyncBridgeTestWithBenefitSyncDisabled,
       MergeFullSyncData_SameWalletCardAndCardBenefit) {
  // Get a card and a benefit, and connect them.
  CreditCard card = test::GetMaskedServerCard2();
  CreditCardBenefit card_benefit = test::GetActiveCreditCardMerchantBenefit();
  LinkCardAndBenefit(card, card_benefit);

  // Get specifics for syncing.
  AutofillWalletSpecifics card_specifics =
      GetSpecificsFromCardAndBenefit(card, card_benefit);

  // Both of client card and benefit should be updated to delete benefit
  // related data.
  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(),
              NotifyOfCreditCardChanged(AddChange(card.server_id(), card)));
  //  We need to commit the updated progress marker on the client even with the
  //  same data.
  EXPECT_CALL(*backend(), CommitChanges()).Times(2);

  StartSyncing({card_specifics});
  MergeFullSyncDataForCards({card_specifics});

  // Benefit related data should be deleted when the benefit syncing feature
  // is disabled.
  card_specifics.mutable_masked_card()->clear_product_terms_url();
  card_specifics.mutable_masked_card()->clear_card_benefit();

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AutofillWalletSyncBridgeTest, ApplyDisableSyncChanges_BankAccount) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  // Create one bank account on the client.
  table()->SetMaskedBankAccounts(
      {test::CreatePixBankAccount(/*instrument_id=*/1234)});

  EXPECT_CALL(*backend(), CommitChanges());
  EXPECT_CALL(*backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_CALL(*backend(), NotifyOfIbanChanged).Times(0);

  // ApplyDisableSyncChanges indicates to the bridge that sync is stopping
  // because it was disabled.
  bridge()->ApplyDisableSyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>());

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Tests that when the server sends the same data as the client has, nothing
// changes on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_SameBankAccountData) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  // Create one bank account on the client.
  BankAccount existing_bank_account =
      test::CreatePixBankAccount(/*instrument_id=*/1234);
  EXPECT_TRUE(table()->SetMaskedBankAccounts({existing_bank_account}));
  std::vector<BankAccount> bank_accounts;
  table()->GetMaskedBankAccounts(bank_accounts);
  ASSERT_EQ(1U, bank_accounts.size());

  // Create the bank account on the server.
  AutofillWalletSpecifics bank_account_specifics;
  SetAutofillWalletSpecificsFromBankAccount(existing_bank_account,
                                            &bank_account_specifics);

  StartSyncing({bank_account_specifics});

  table()->GetMaskedBankAccounts(bank_accounts);
  EXPECT_EQ(1U, bank_accounts.size());
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(bank_account_specifics)));
}

// Tests that when the server sends a new bank account, it gets added to the
// database.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NewBankAccount) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  // Create one bank account on the client.
  BankAccount existing_bank_account =
      test::CreatePixBankAccount(/*instrument_id=*/1234);
  EXPECT_TRUE(table()->SetMaskedBankAccounts({existing_bank_account}));
  std::vector<BankAccount> bank_accounts;
  table()->GetMaskedBankAccounts(bank_accounts);
  ASSERT_EQ(1U, bank_accounts.size());
  AutofillWalletSpecifics existing_bank_account_specifics;
  SetAutofillWalletSpecificsFromBankAccount(existing_bank_account,
                                            &existing_bank_account_specifics);

  // Create the bank account on the server.
  BankAccount new_bank_account =
      test::CreatePixBankAccount(/*instrument_id=*/9999);
  AutofillWalletSpecifics new_bank_account_specifics;
  SetAutofillWalletSpecificsFromBankAccount(new_bank_account,
                                            &new_bank_account_specifics);

  StartSyncing({new_bank_account_specifics, existing_bank_account_specifics});

  table()->GetMaskedBankAccounts(bank_accounts);
  EXPECT_EQ(2U, bank_accounts.size());
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(existing_bank_account_specifics),
                           EqualsSpecifics(new_bank_account_specifics)));
}

// Tests that when the server sends an updated bank account, it gets updated in
// the database.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_UpdatedBankAccount) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  // Create one bank account on the client.
  BankAccount existing_bank_account =
      test::CreatePixBankAccount(/*instrument_id=*/1234);
  EXPECT_TRUE(table()->SetMaskedBankAccounts({existing_bank_account}));
  std::vector<BankAccount> bank_accounts;
  table()->GetMaskedBankAccounts(bank_accounts);
  ASSERT_EQ(1U, bank_accounts.size());

  // Update the bank account on the server. Only the instrument id is the same
  // as the existing bank account.
  BankAccount udpated_bank_account(
      existing_bank_account.payment_instrument().instrument_id(),
      u"updated_nickname", GURL("http://www.example-updated.com"),
      u"updated_bank_name", u"account_number_suffix_updated",
      BankAccount::AccountType::kSavings);

  AutofillWalletSpecifics updated_bank_account_specifics;
  SetAutofillWalletSpecificsFromBankAccount(udpated_bank_account,
                                            &updated_bank_account_specifics);

  StartSyncing({updated_bank_account_specifics});

  table()->GetMaskedBankAccounts(bank_accounts);
  EXPECT_EQ(1U, bank_accounts.size());
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(updated_bank_account_specifics)));
}

// Tests that when the server deletes a bank account, it gets removed from the
// database.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_RemoveBankAccount) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  // Create one bank account on the client.
  BankAccount bank_account_1 =
      test::CreatePixBankAccount(/*instrument_id=*/1234);
  BankAccount bank_account_2 =
      test::CreatePixBankAccount(/*instrument_id=*/9999);
  EXPECT_TRUE(table()->SetMaskedBankAccounts({bank_account_1, bank_account_2}));
  std::vector<BankAccount> bank_accounts;
  table()->GetMaskedBankAccounts(bank_accounts);
  ASSERT_EQ(2U, bank_accounts.size());
  AutofillWalletSpecifics bank_account_1_specifics;
  SetAutofillWalletSpecificsFromBankAccount(bank_account_1,
                                            &bank_account_1_specifics);

  // Remove bank account 2 on the server.
  StartSyncing({bank_account_1_specifics});

  table()->GetMaskedBankAccounts(bank_accounts);
  EXPECT_EQ(1U, bank_accounts.size());
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(bank_account_1_specifics)));
}

// Tests that when the server sends a new bank account, it does not get added to
// the database if the experiment is off.
TEST_F(AutofillWalletSyncBridgeTest, MergeFullSyncData_NewBankAccount_ExpOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  // Create a bank account on the server.
  BankAccount bank_account = test::CreatePixBankAccount(/*instrument_id=*/1234);
  std::vector<BankAccount> bank_accounts;
  AutofillWalletSpecifics bank_account_specifics;
  SetAutofillWalletSpecificsFromBankAccount(bank_account,
                                            &bank_account_specifics);

  StartSyncing({bank_account_specifics});

  table()->GetMaskedBankAccounts(bank_accounts);
  EXPECT_EQ(0U, bank_accounts.size());
}

// Tests that when the server sends the same data as the client has, nothing
// changes on the client.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_SameEwalletPaymentInstrumentData) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  // Create one ewallet payment instrument on the client.
  sync_pb::PaymentInstrument existing_ewallet_payment_instrument =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  EXPECT_TRUE(
      table()->SetPaymentInstruments({existing_ewallet_payment_instrument}));
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  table()->GetPaymentInstruments(payment_instruments);
  ASSERT_EQ(1U, payment_instruments.size());

  // Create the eWallet payment instrument on the server.
  AutofillWalletSpecifics ewallet_account_specifics;
  SetAutofillWalletSpecificsFromPaymentInstrument(
      existing_ewallet_payment_instrument, ewallet_account_specifics);

  StartSyncing({ewallet_account_specifics});

  table()->GetPaymentInstruments(payment_instruments);
  EXPECT_EQ(1U, payment_instruments.size());
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(ewallet_account_specifics)));
}

// Tests that when the server sends a new ewallet account, it gets added to the
// database.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_NewEwalletPaymentInstrument) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  // Create one ewallet payment instrument on the client.
  sync_pb::PaymentInstrument existing_ewallet_payment_instrument =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  EXPECT_TRUE(
      table()->SetPaymentInstruments({existing_ewallet_payment_instrument}));
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  table()->GetPaymentInstruments(payment_instruments);
  ASSERT_EQ(1U, payment_instruments.size());
  AutofillWalletSpecifics existing_ewallet_account_specifics;
  SetAutofillWalletSpecificsFromPaymentInstrument(
      existing_ewallet_payment_instrument, existing_ewallet_account_specifics);

  // Create the ewallet payment instrument on the server.
  sync_pb::PaymentInstrument new_ewallet_payment_instrument =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/9999);
  AutofillWalletSpecifics new_ewallet_account_specifics;
  SetAutofillWalletSpecificsFromPaymentInstrument(
      new_ewallet_payment_instrument, new_ewallet_account_specifics);

  StartSyncing(
      {new_ewallet_account_specifics, existing_ewallet_account_specifics});

  table()->GetPaymentInstruments(payment_instruments);
  EXPECT_EQ(2U, payment_instruments.size());
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(existing_ewallet_account_specifics),
                           EqualsSpecifics(new_ewallet_account_specifics)));
}

// Tests that when the server sends an updated ewallet account, it gets updated
// in the database.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_UpdatedEwalletPaymentInstrument) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  // Create one ewallet payment instrument on the client.
  sync_pb::PaymentInstrument existing_ewallet_payment_instrument =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  EXPECT_TRUE(
      table()->SetPaymentInstruments({existing_ewallet_payment_instrument}));
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  table()->GetPaymentInstruments(payment_instruments);
  ASSERT_EQ(1U, payment_instruments.size());

  // Update the ewallet payment instrument on the server. Only the instrument id
  // is the same as the existing ewallet payment instrument.
  sync_pb::PaymentInstrument udpated_ewallet_payment_instrument;
  udpated_ewallet_payment_instrument.set_instrument_id(1234);
  sync_pb::EwalletDetails* ewallet =
      udpated_ewallet_payment_instrument.mutable_ewallet_details();
  ewallet->set_ewallet_name("updated_ewallet_name");
  ewallet->set_account_display_name("updated_account_display_name");

  AutofillWalletSpecifics updated_ewallet_account_specifics;
  SetAutofillWalletSpecificsFromPaymentInstrument(
      udpated_ewallet_payment_instrument, updated_ewallet_account_specifics);

  StartSyncing({updated_ewallet_account_specifics});

  table()->GetPaymentInstruments(payment_instruments);
  EXPECT_EQ(1U, payment_instruments.size());
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(EqualsSpecifics(updated_ewallet_account_specifics)));
}

// Tests that when the server deletes a eWallet payment instrument, it gets
// removed from the database.
TEST_F(AutofillWalletSyncBridgeTest,
       MergeFullSyncData_RemoveEwalletPaymentInstrument) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  // Create two ewallet payment instruments on the client.
  sync_pb::PaymentInstrument ewallet_payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  sync_pb::PaymentInstrument ewallet_payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/9999);
  EXPECT_TRUE(table()->SetPaymentInstruments(
      {ewallet_payment_instrument_1, ewallet_payment_instrument_2}));
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  table()->GetPaymentInstruments(payment_instruments);
  ASSERT_EQ(2U, payment_instruments.size());

  AutofillWalletSpecifics ewallet_payment_instrument_1_specifics;
  SetAutofillWalletSpecificsFromPaymentInstrument(
      ewallet_payment_instrument_1, ewallet_payment_instrument_1_specifics);

  // Remove ewallet payment instrument 2 on the server.
  StartSyncing({ewallet_payment_instrument_1_specifics});

  table()->GetPaymentInstruments(payment_instruments);
  EXPECT_EQ(1U, payment_instruments.size());
  EXPECT_THAT(GetAllLocalData(), UnorderedElementsAre(EqualsSpecifics(
                                     ewallet_payment_instrument_1_specifics)));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace autofill
