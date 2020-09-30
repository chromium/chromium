// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using syncer::EntityChange;
using syncer::EntityData;

class TestAutofillTable : public AutofillTable {
 public:
  explicit TestAutofillTable(std::vector<CreditCard> cards_on_disk)
      : cards_on_disk_(cards_on_disk) {}

  ~TestAutofillTable() override {}

  bool GetServerCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* cards) const override {
    for (const auto& card_on_disk : cards_on_disk_)
      cards->push_back(std::make_unique<CreditCard>(card_on_disk));
    return true;
  }

 private:
  std::vector<CreditCard> cards_on_disk_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillTable);
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
  ~AutofillSyncBridgeUtilTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillSyncBridgeUtilTest);
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

  // Verify that the card_issuer is set correctly.
  EXPECT_EQ(wallet_cards.front().card_issuer(), CreditCard::ISSUER_UNKNOWN);
  EXPECT_EQ(wallet_cards.back().card_issuer(), CreditCard::GOOGLE);
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

// Test to ensure the an AutofillOfferData is correctly converted to an
// AutofillOfferSpecifics.
TEST_F(AutofillSyncBridgeUtilTest, OfferSpecificsFromOfferData) {
  sync_pb::AutofillOfferSpecifics offer_specifics;
  AutofillOfferData offer_data = test::GetCardLinkedOfferData1();
  SetAutofillOfferSpecificsFromOfferData(offer_data, &offer_specifics);

  EXPECT_EQ(offer_specifics.id(), offer_data.offer_id);
  EXPECT_EQ(offer_specifics.offer_details_url(), offer_data.offer_details_url);
  EXPECT_EQ(offer_specifics.offer_expiry_date(),
            (offer_data.expiry - base::Time::UnixEpoch()).InSeconds());
  EXPECT_TRUE(offer_specifics.percentage_reward().percentage() ==
                  offer_data.offer_reward_amount ||
              offer_specifics.fixed_amount_reward().amount() ==
                  offer_data.offer_reward_amount);
  EXPECT_EQ(offer_specifics.merchant_domain().size(),
            (int)offer_data.merchant_domain.size());
  for (int i = 0; i < offer_specifics.merchant_domain().size(); i++) {
    EXPECT_EQ(offer_specifics.merchant_domain(i),
              offer_data.merchant_domain[i].GetOrigin().spec());
  }
  EXPECT_EQ(offer_specifics.card_linked_offer_data().instrument_id().size(),
            (int)offer_data.eligible_instrument_id.size());
  for (int i = 0;
       i < offer_specifics.card_linked_offer_data().instrument_id().size();
       i++) {
    EXPECT_EQ(offer_specifics.card_linked_offer_data().instrument_id(i),
              offer_data.eligible_instrument_id[i]);
  }
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

  new_offer_data.at(0).offer_id += 456;
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
  // Expects default specifics is valid.
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
  // Expects specifics without linked card instrument id to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));
  specifics.clear_card_linked_offer_data();
  // Expects specifics without card linked offer data to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));

  SetAutofillOfferSpecificsFromOfferData(test::GetCardLinkedOfferData1(),
                                         &specifics);
  specifics.mutable_percentage_reward()->set_percentage("5");
  // Expects specifics without correct reward text to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));
  specifics.clear_percentage_reward();
  // Expects specifics without reward text to be invalid.
  EXPECT_FALSE(IsOfferSpecificsValid(specifics));
  specifics.mutable_fixed_amount_reward()->set_amount("$5");
  // Expects specifics with only fixed amount reward text to be valid.
  EXPECT_TRUE(IsOfferSpecificsValid(specifics));
}

}  // namespace
}  // namespace autofill
