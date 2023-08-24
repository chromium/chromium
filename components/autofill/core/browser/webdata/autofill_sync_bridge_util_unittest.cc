// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/autofill_offer_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/autofill_wallet_usage_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using syncer::EntityChange;
using syncer::EntityData;

class TestAutofillTable : public AutofillTable {
 public:
  explicit TestAutofillTable(std::vector<CreditCard> cards_on_disk)
      : cards_on_disk_(cards_on_disk) {}

  TestAutofillTable(const TestAutofillTable&) = delete;
  TestAutofillTable& operator=(const TestAutofillTable&) = delete;

  ~TestAutofillTable() override {}

  bool GetServerCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* cards) const override {
    for (const auto& card_on_disk : cards_on_disk_)
      cards->push_back(std::make_unique<CreditCard>(card_on_disk));
    return true;
  }

 private:
  std::vector<CreditCard> cards_on_disk_;
};

EntityData SpecificsToEntity(const sync_pb::AutofillWalletSpecifics& specifics,
                             const std::string& client_tag) {
  syncer::EntityData data;
  *data.specifics.mutable_autofill_wallet() = specifics;
  data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
      syncer::AUTOFILL_WALLET_DATA, client_tag);
  return data;
}

class AutofillSyncBridgeUtilTest : public testing::Test {
 public:
  AutofillSyncBridgeUtilTest() {}

  AutofillSyncBridgeUtilTest(const AutofillSyncBridgeUtilTest&) = delete;
  AutofillSyncBridgeUtilTest& operator=(const AutofillSyncBridgeUtilTest&) =
      delete;

  ~AutofillSyncBridgeUtilTest() override {}
};

// Tests that PopulateWalletTypesFromSyncData behaves as expected.
TEST_F(AutofillSyncBridgeUtilTest, PopulateWalletTypesFromSyncData) {
  // Add an address first.
  syncer::EntityChangeList entity_data;
  std::string address_id("address1");
  entity_data.push_back(EntityChange::CreateAdd(
      address_id,
      SpecificsToEntity(CreateAutofillWalletSpecificsForAddress(address_id),
                        /*client_tag=*/"address-address1")));
  // Add two credit cards.
  std::string credit_card_id_1 = "credit_card_1";
  std::string credit_card_id_2 = "credit_card_2";
  // Add the first card that has its billing address id set to the address's id.
  // No nickname is set.
  sync_pb::AutofillWalletSpecifics wallet_specifics_card1 =
      CreateAutofillWalletSpecificsForCard(
          /*id=*/credit_card_id_1,
          /*billing_address_id=*/address_id);
  wallet_specifics_card1.mutable_masked_card()
      ->set_virtual_card_enrollment_state(
          sync_pb::WalletMaskedCreditCard::UNENROLLED);
  wallet_specifics_card1.mutable_masked_card()
      ->mutable_card_issuer()
      ->set_issuer_id("amex");
  // Add the second card that has nickname.
  std::string nickname("Grocery card");
  sync_pb::AutofillWalletSpecifics wallet_specifics_card2 =
      CreateAutofillWalletSpecificsForCard(
          /*id=*/credit_card_id_2,
          /*billing_address_id=*/"", /*nickname=*/nickname);
  // Set the second card's issuer to GOOGLE.
  wallet_specifics_card2.mutable_masked_card()
      ->mutable_card_issuer()
      ->set_issuer(sync_pb::CardIssuer::GOOGLE);
  wallet_specifics_card2.mutable_masked_card()
      ->mutable_card_issuer()
      ->set_issuer_id("google");
  wallet_specifics_card2.mutable_masked_card()
      ->set_virtual_card_enrollment_state(
          sync_pb::WalletMaskedCreditCard::ENROLLED);
  wallet_specifics_card2.mutable_masked_card()
      ->set_virtual_card_enrollment_type(
          sync_pb::WalletMaskedCreditCard::NETWORK);
  wallet_specifics_card2.mutable_masked_card()->set_card_art_url(
      "https://www.example.com/card.png");
  wallet_specifics_card2.mutable_masked_card()->set_product_description(
      "fake product description");
  entity_data.push_back(EntityChange::CreateAdd(
      credit_card_id_1,
      SpecificsToEntity(wallet_specifics_card1, /*client_tag=*/"card-card1")));
  entity_data.push_back(EntityChange::CreateAdd(
      credit_card_id_2,
      SpecificsToEntity(wallet_specifics_card2, /*client_tag=*/"card-card2")));
  // Add payments customer data.
  entity_data.push_back(EntityChange::CreateAdd(
      "deadbeef",
      SpecificsToEntity(CreateAutofillWalletSpecificsForPaymentsCustomerData(
                            /*specifics_id=*/"deadbeef"),
                        /*client_tag=*/"customer-deadbeef")));
  // Add cloud token data.
  entity_data.push_back(EntityChange::CreateAdd(
      "data1", SpecificsToEntity(
                   CreateAutofillWalletSpecificsForCreditCardCloudTokenData(
                       /*specifics_id=*/"data1"),
                   /*client_tag=*/"token-data1")));

  std::vector<CreditCard> wallet_cards;
  std::vector<AutofillProfile> wallet_addresses;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  PopulateWalletTypesFromSyncData(entity_data, &wallet_cards, &wallet_addresses,
                                  &customer_data, &cloud_token_data);

  ASSERT_EQ(2U, wallet_cards.size());
  ASSERT_EQ(1U, wallet_addresses.size());

  EXPECT_EQ("deadbeef", customer_data.back().customer_id);

  EXPECT_EQ("data1", cloud_token_data.back().instrument_token);

  // Make sure the first card's billing address id is equal to the address'
  // server id.
  EXPECT_EQ(wallet_addresses.back().server_id(),
            wallet_cards.front().billing_address_id());
  // The first card's nickname is empty.
  EXPECT_TRUE(wallet_cards.front().nickname().empty());

  // Make sure the second card's nickname is correctly populated from sync data.
  EXPECT_EQ(base::UTF8ToUTF16(nickname), wallet_cards.back().nickname());

  // Verify that the issuer and the issuer id are set correctly.
  EXPECT_EQ(wallet_cards.front().card_issuer(),
            CreditCard::Issuer::kExternalIssuer);
  EXPECT_EQ(wallet_cards.front().issuer_id(), "amex");
  EXPECT_EQ(wallet_cards.back().card_issuer(), CreditCard::Issuer::kGoogle);
  EXPECT_EQ(wallet_cards.back().issuer_id(), "google");

  // Verify that the virtual_card_enrollment_state is set correctly.
  EXPECT_EQ(wallet_cards.front().virtual_card_enrollment_state(),
            CreditCard::VirtualCardEnrollmentState::kUnenrolled);
  EXPECT_EQ(wallet_cards.back().virtual_card_enrollment_state(),
            CreditCard::VirtualCardEnrollmentState::kEnrolled);

  // Verify that the virtual_card_enrollment_type is set correctly.
  EXPECT_EQ(wallet_cards.back().virtual_card_enrollment_type(),
            CreditCard::VirtualCardEnrollmentType::kNetwork);

  // Verify that the card_art_url is set correctly.
  EXPECT_TRUE(wallet_cards.front().card_art_url().is_empty());
  EXPECT_EQ(wallet_cards.back().card_art_url().spec(),
            "https://www.example.com/card.png");

  // Verify that the product_description is set correctly.
  EXPECT_TRUE(wallet_cards.front().product_description().empty());
  EXPECT_EQ(wallet_cards.back().product_description(),
            u"fake product description");
}

// Verify that the billing address id from the card saved on disk is kept if it
// is a local profile guid.
TEST_F(AutofillSyncBridgeUtilTest,
       CopyRelevantWalletMetadataFromDisk_KeepLocalAddresses) {
  std::vector<CreditCard> cards_on_disk;
  std::vector<CreditCard> wallet_cards;

  // Create a local profile to be used as a billing address.
  AutofillProfile billing_address;

  // Create a card on disk that refers to that local profile as its billing
  // address.
  cards_on_disk.push_back(CreditCard());
  cards_on_disk.back().set_billing_address_id(billing_address.guid());

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.push_back(CreditCard(cards_on_disk.back()));
  wallet_cards.back().set_billing_address_id("1234");

  // Setup the TestAutofillTable with the cards_on_disk.
  TestAutofillTable table(cards_on_disk);

  CopyRelevantWalletMetadataFromDisk(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the wallet card replace its billing address id for the one that
  // was saved on disk.
  EXPECT_EQ(cards_on_disk.back().billing_address_id(),
            wallet_cards.back().billing_address_id());
}

// Verify that the billing address id from the card saved on disk is overwritten
// if it does not refer to a local profile.
TEST_F(AutofillSyncBridgeUtilTest,
       CopyRelevantWalletMetadataFromDisk_OverwriteOtherAddresses) {
  std::string old_billing_id = "1234";
  std::string new_billing_id = "9876";
  std::vector<CreditCard> cards_on_disk;
  std::vector<CreditCard> wallet_cards;

  // Create a card on disk that does not refer to a local profile (which have 36
  // chars ids).
  cards_on_disk.push_back(CreditCard());
  cards_on_disk.back().set_billing_address_id(old_billing_id);

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.push_back(CreditCard(cards_on_disk.back()));
  wallet_cards.back().set_billing_address_id(new_billing_id);

  // Setup the TestAutofillTable with the cards_on_disk.
  TestAutofillTable table(cards_on_disk);

  CopyRelevantWalletMetadataFromDisk(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the local address billing id that was saved on disk did not
  // replace the new one.
  EXPECT_EQ(new_billing_id, wallet_cards.back().billing_address_id());
}

// Verify that the use stats on disk are kept when server cards are synced.
TEST_F(AutofillSyncBridgeUtilTest,
       CopyRelevantWalletMetadataFromDisk_KeepUseStats) {
  TestAutofillClock test_clock;
  base::Time arbitrary_time = base::Time::FromDoubleT(25);
  base::Time disk_time = base::Time::FromDoubleT(10);
  test_clock.SetNow(arbitrary_time);

  std::vector<CreditCard> cards_on_disk;
  std::vector<CreditCard> wallet_cards;

  // Create a card on disk with specific use stats.
  cards_on_disk.push_back(CreditCard());
  cards_on_disk.back().set_use_count(3U);
  cards_on_disk.back().set_use_date(disk_time);

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.push_back(CreditCard());
  wallet_cards.back().set_use_count(10U);

  // Setup the TestAutofillTable with the cards_on_disk.
  TestAutofillTable table(cards_on_disk);

  CopyRelevantWalletMetadataFromDisk(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the use stats from disk were kept
  EXPECT_EQ(3U, wallet_cards.back().use_count());
  EXPECT_EQ(disk_time, wallet_cards.back().use_date());
}

// Test to ensure the general-purpose fields from an AutofillOfferData are
// correctly converted to an AutofillOfferSpecifics.
TEST_F(AutofillSyncBridgeUtilTest, OfferSpecificsFromOfferData) {
  sync_pb::AutofillOfferSpecifics offer_specifics;
  AutofillOfferData offer_data = test::GetCardLinkedOfferData1();
  SetAutofillOfferSpecificsFromOfferData(offer_data, &offer_specifics);

  EXPECT_EQ(offer_specifics.id(), offer_data.GetOfferId());
  EXPECT_EQ(offer_specifics.offer_details_url(),
            offer_data.GetOfferDetailsUrl());
  EXPECT_EQ(offer_specifics.offer_expiry_date(),
            (offer_data.GetExpiry() - base::Time::UnixEpoch()).InSeconds());
  EXPECT_EQ(offer_specifics.merchant_domain().size(),
            (int)offer_data.GetMerchantOrigins().size());
  for (int i = 0; i < offer_specifics.merchant_domain().size(); i++) {
    EXPECT_EQ(offer_specifics.merchant_domain(i),
              offer_data.GetMerchantOrigins()[i].spec());
  }
  EXPECT_EQ(offer_specifics.display_strings().value_prop_text(),
            offer_data.GetDisplayStrings().value_prop_text);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(offer_specifics.display_strings().see_details_text_mobile(),
            offer_data.GetDisplayStrings().see_details_text);
  EXPECT_EQ(offer_specifics.display_strings().usage_instructions_text_mobile(),
            offer_data.GetDisplayStrings().usage_instructions_text);
#else
  EXPECT_EQ(offer_specifics.display_strings().see_details_text_desktop(),
            offer_data.GetDisplayStrings().see_details_text);
  EXPECT_EQ(offer_specifics.display_strings().usage_instructions_text_desktop(),
            offer_data.GetDisplayStrings().usage_instructions_text);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

// Test to ensure the card-linked offer-specific fields from an
// AutofillOfferData are correctly converted to an AutofillOfferSpecifics.
TEST_F(AutofillSyncBridgeUtilTest, OfferSpecificsFromCardLinkedOfferData) {
  sync_pb::AutofillOfferSpecifics offer_specifics;
  AutofillOfferData offer_data = test::GetCardLinkedOfferData1();
  SetAutofillOfferSpecificsFromOfferData(offer_data, &offer_specifics);

  EXPECT_TRUE(offer_specifics.percentage_reward().percentage() ==
                  offer_data.GetOfferRewardAmount() ||
              offer_specifics.fixed_amount_reward().amount() ==
                  offer_data.GetOfferRewardAmount());
  EXPECT_EQ(offer_specifics.card_linked_offer_data().instrument_id().size(),
            (int)offer_data.GetEligibleInstrumentIds().size());
  for (int i = 0;
       i < offer_specifics.card_linked_offer_data().instrument_id().size();
       i++) {
    EXPECT_EQ(offer_specifics.card_linked_offer_data().instrument_id(i),
              offer_data.GetEligibleInstrumentIds()[i]);
  }
}

// Test to ensure the promo code offer-specific fields from an AutofillOfferData
// are correctly converted to an AutofillOfferSpecifics.
TEST_F(AutofillSyncBridgeUtilTest, OfferSpecificsFromPromoCodeOfferData) {
  sync_pb::AutofillOfferSpecifics offer_specifics;
  AutofillOfferData offer_data = test::GetPromoCodeOfferData();
  SetAutofillOfferSpecificsFromOfferData(offer_data, &offer_specifics);

  EXPECT_EQ(offer_specifics.promo_code_offer_data().promo_code(),
            offer_data.GetPromoCode());
}

// Ensures that the ShouldResetAutofillWalletData function works correctly, if
// the two given data sets have the same size.
TEST_F(AutofillSyncBridgeUtilTest,
       ShouldResetAutofillWalletData_SameDataSetSize) {
  std::vector<std::unique_ptr<AutofillOfferData>> old_offer_data;
  std::vector<AutofillOfferData> new_offer_data;

  AutofillOfferData data1 = test::GetCardLinkedOfferData1();
  AutofillOfferData data2 = test::GetCardLinkedOfferData2();
  old_offer_data.push_back(std::make_unique<AutofillOfferData>(data1));
  new_offer_data.push_back(data2);
  old_offer_data.push_back(std::make_unique<AutofillOfferData>(data2));
  new_offer_data.push_back(data1);
  EXPECT_FALSE(AreAnyItemsDifferent(old_offer_data, new_offer_data));

  new_offer_data.at(0).SetOfferIdForTesting(new_offer_data.at(0).GetOfferId() +
                                            456);
  EXPECT_TRUE(AreAnyItemsDifferent(old_offer_data, new_offer_data));
}

// Ensures that the ShouldResetAutofillWalletData function works correctly, if
// the two given data sets have different size.
TEST_F(AutofillSyncBridgeUtilTest,
       ShouldResetAutofillWalletData_DifferentDataSetSize) {
  std::vector<std::unique_ptr<AutofillOfferData>> old_offer_data;
  std::vector<AutofillOfferData> new_offer_data;

  AutofillOfferData data1 = test::GetCardLinkedOfferData1();
  AutofillOfferData data2 = test::GetCardLinkedOfferData2();
  old_offer_data.push_back(std::make_unique<AutofillOfferData>(data1));
  new_offer_data.push_back(data2);
  new_offer_data.push_back(data1);
  EXPECT_TRUE(AreAnyItemsDifferent(old_offer_data, new_offer_data));
}

// Ensures that function IsOfferSpecificsValid is working correctly.
TEST_F(AutofillSyncBridgeUtilTest, IsOfferSpecificsValid) {
  sync_pb::AutofillOfferSpecifics specifics;
  SetAutofillOfferSpecificsFromOfferData(test::GetCardLinkedOfferData1(),
                                         &specifics);
  // Expects default card-linked offer specifics is valid.
  EXPECT_TRUE(IsOfferSpecificsValid(specifics));

  specifics.clear_id();
  // Expects specifics without id to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));

  SetAutofillOfferSpecificsFromOfferData(test::GetCardLinkedOfferData1(),
                                         &specifics);
  specifics.clear_merchant_domain();
  // Expects specifics without merchant domain to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));
  specifics.add_merchant_domain("invalid url");
  // Expects specifics with an invalid merchant_domain to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));

  SetAutofillOfferSpecificsFromOfferData(test::GetCardLinkedOfferData1(),
                                         &specifics);
  specifics.mutable_card_linked_offer_data()->clear_instrument_id();
  // Expects card-linked offer specifics without linked card instrument id to be
  // invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));
  specifics.clear_card_linked_offer_data();
  // Expects specifics without card linked offer data or promo code offer data
  // to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));

  SetAutofillOfferSpecificsFromOfferData(test::GetCardLinkedOfferData1(),
                                         &specifics);
  specifics.mutable_percentage_reward()->set_percentage("5");
  // Expects card-linked offer specifics without correct reward text to be
  // invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));
  specifics.clear_percentage_reward();
  // Expects card-linked offer specifics without reward text to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));
  specifics.mutable_fixed_amount_reward()->set_amount("$5");
  // Expects card-linked offer specifics with only fixed amount reward text to
  // be valid.
  EXPECT_TRUE(IsOfferSpecificsValid(specifics));

  SetAutofillOfferSpecificsFromOfferData(test::GetPromoCodeOfferData(),
                                         &specifics);
  // Expects default promo code offer specifics is valid.
  EXPECT_TRUE(IsOfferSpecificsValid(specifics));
  // Expects promo code offer specifics without promo code to be invalid.
  specifics.mutable_promo_code_offer_data()->clear_promo_code();
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));
}

// Test to ensure that Wallet Usage Data for virtual card retrieval is correctly
// converted to AutofillWalletUsageSpecifics.
TEST_F(AutofillSyncBridgeUtilTest, WalletUsageSpecificsFromWalletUsageData) {
  sync_pb::AutofillWalletUsageSpecifics usage_specifics;
  AutofillWalletUsageData usage_data =
      AutofillWalletUsageData::ForVirtualCard(test::GetVirtualCardUsageData1());
  SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(usage_data,
                                                             &usage_specifics);

  EXPECT_EQ(usage_specifics.guid(),
            *usage_data.virtual_card_usage_data().usage_data_id());
  EXPECT_EQ(usage_specifics.virtual_card_usage_data().instrument_id(),
            *usage_data.virtual_card_usage_data().instrument_id());
  EXPECT_EQ(
      base::UTF8ToUTF16(
          usage_specifics.virtual_card_usage_data().virtual_card_last_four()),
      *usage_data.virtual_card_usage_data().virtual_card_last_four());
  EXPECT_EQ(usage_specifics.virtual_card_usage_data().merchant_url(),
            usage_data.virtual_card_usage_data().merchant_origin().Serialize());
}

// Test to ensure that Wallet Usage Data for virtual card retrieval is correctly
// converted to AutofillWalletUsageSpecifics.
TEST_F(AutofillSyncBridgeUtilTest, VirtualCardUsageDataFromUsageSpecifics) {
  sync_pb::AutofillWalletUsageSpecifics usage_specifics;
  SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(
      AutofillWalletUsageData::ForVirtualCard(test::GetVirtualCardUsageData1()),
      &usage_specifics);

  VirtualCardUsageData virtual_card_usage_data =
      VirtualCardUsageDataFromUsageSpecifics(usage_specifics);

  EXPECT_EQ(*virtual_card_usage_data.usage_data_id(), usage_specifics.guid());
  EXPECT_EQ(*virtual_card_usage_data.instrument_id(),
            usage_specifics.virtual_card_usage_data().instrument_id());
  EXPECT_EQ(
      base::UTF16ToUTF8(*virtual_card_usage_data.virtual_card_last_four()),
      usage_specifics.virtual_card_usage_data().virtual_card_last_four());
  EXPECT_EQ(virtual_card_usage_data.merchant_origin().Serialize(),
            usage_specifics.virtual_card_usage_data().merchant_url());
}

// Test to ensure that WalletCredential struct data for CVV storage is correctly
// converted to AutofillWalletCredentialSpecifics.
TEST_F(AutofillSyncBridgeUtilTest,
       AutofillWalletCredentialSpecificsFromStructData) {
  std::unique_ptr<ServerCvc> server_cvc = std::make_unique<ServerCvc>(
      1234, u"890", base::Time::UnixEpoch() + base::Milliseconds(25000));

  sync_pb::AutofillWalletCredentialSpecifics wallet_credential_specifics =
      AutofillWalletCredentialSpecificsFromStructData(*server_cvc);

  EXPECT_EQ(base::NumberToString(server_cvc->instrument_id),
            wallet_credential_specifics.instrument_id());
  EXPECT_EQ(base::UTF16ToUTF8(server_cvc->cvc),
            wallet_credential_specifics.cvc());
  EXPECT_EQ((server_cvc->last_updated_timestamp - base::Time::UnixEpoch())
                .InMilliseconds(),
            wallet_credential_specifics.last_updated_time_unix_epoch_millis());
  EXPECT_EQ((server_cvc->last_updated_timestamp - base::Time::UnixEpoch())
                .InMilliseconds(),
            25000);
}

TEST_F(AutofillSyncBridgeUtilTest, AutofillWalletStructDataFromUsageSpecifics) {
  sync_pb::AutofillWalletCredentialSpecifics wallet_credential_specifics;
  wallet_credential_specifics.set_instrument_id("123");
  wallet_credential_specifics.set_cvc("890");
  wallet_credential_specifics.set_last_updated_time_unix_epoch_millis(
      base::Milliseconds(25000).InMilliseconds());

  ServerCvc server_cvc =
      AutofillWalletCvcStructDataFromWalletCredentialSpecifics(
          wallet_credential_specifics);

  EXPECT_EQ(base::NumberToString(server_cvc.instrument_id),
            wallet_credential_specifics.instrument_id());
  EXPECT_EQ(base::UTF16ToUTF8(server_cvc.cvc),
            wallet_credential_specifics.cvc());
  EXPECT_EQ((server_cvc.last_updated_timestamp - base::Time::UnixEpoch())
                .InMilliseconds(),
            wallet_credential_specifics.last_updated_time_unix_epoch_millis());
  EXPECT_EQ((server_cvc.last_updated_timestamp - base::Time::UnixEpoch())
                .InMilliseconds(),
            25000);
}

// Round trip test to ensure that WalletCredential struct data for CVV storage
// is correctly converted to AutofillWalletCredentialSpecifics and then from
// the converted AutofillWalletCredentialSpecifics to WalletCredential struct
// data. In the end we compare the original and the converted struct data.
TEST_F(AutofillSyncBridgeUtilTest,
       AutofillWalletCredentialStructDataRoundTripTest) {
  // Step 1 - Convert WalletCredential struct data to
  // AutofillWalletCredentialSpecifics.
  std::unique_ptr<ServerCvc> server_cvc = std::make_unique<ServerCvc>(
      1234, u"890", base::Time::UnixEpoch() + base::Milliseconds(25000));

  sync_pb::AutofillWalletCredentialSpecifics
      wallet_credential_specifics_from_conversion =
          AutofillWalletCredentialSpecificsFromStructData(*server_cvc);

  EXPECT_EQ(base::NumberToString(server_cvc->instrument_id),
            wallet_credential_specifics_from_conversion.instrument_id());
  EXPECT_EQ(base::UTF16ToUTF8(server_cvc->cvc),
            wallet_credential_specifics_from_conversion.cvc());
  EXPECT_EQ((server_cvc->last_updated_timestamp - base::Time::UnixEpoch())
                .InMilliseconds(),
            wallet_credential_specifics_from_conversion
                .last_updated_time_unix_epoch_millis());
  EXPECT_EQ((server_cvc->last_updated_timestamp - base::Time::UnixEpoch())
                .InMilliseconds(),
            25000);

  // Step 2 - Convert AutofillWalletCredentialSpecifics to WalletCredential
  // struct data.
  ServerCvc server_cvc_from_conversion =
      AutofillWalletCvcStructDataFromWalletCredentialSpecifics(
          wallet_credential_specifics_from_conversion);

  EXPECT_EQ(base::NumberToString(server_cvc_from_conversion.instrument_id),
            wallet_credential_specifics_from_conversion.instrument_id());
  EXPECT_EQ(base::UTF16ToUTF8(server_cvc_from_conversion.cvc),
            wallet_credential_specifics_from_conversion.cvc());
  EXPECT_EQ((server_cvc_from_conversion.last_updated_timestamp -
             base::Time::UnixEpoch())
                .InMilliseconds(),
            wallet_credential_specifics_from_conversion
                .last_updated_time_unix_epoch_millis());
  EXPECT_EQ((server_cvc_from_conversion.last_updated_timestamp -
             base::Time::UnixEpoch())
                .InMilliseconds(),
            25000);

  // Step 3 - Compare the original WalletCredential struct data to the
  // converted WalletCredential struct data.
  EXPECT_EQ(server_cvc_from_conversion.instrument_id,
            server_cvc->instrument_id);
  EXPECT_EQ(server_cvc_from_conversion.cvc, server_cvc->cvc);
  EXPECT_EQ(server_cvc_from_conversion.last_updated_timestamp,
            server_cvc->last_updated_timestamp);
}

}  // namespace
}  // namespace autofill
