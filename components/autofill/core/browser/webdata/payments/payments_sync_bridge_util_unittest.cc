// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit_test_api.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_test_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
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

class TestPaymentsAutofillTable : public PaymentsAutofillTable {
 public:
  explicit TestPaymentsAutofillTable(std::vector<CreditCard> cards_on_disk)
      : cards_on_disk_(cards_on_disk) {}

  TestPaymentsAutofillTable(const TestPaymentsAutofillTable&) = delete;
  TestPaymentsAutofillTable& operator=(const TestPaymentsAutofillTable&) =
      delete;

  ~TestPaymentsAutofillTable() override {}

  bool GetServerCreditCards(
      std::vector<std::unique_ptr<CreditCard>>& cards) const override {
    for (const auto& card_on_disk : cards_on_disk_)
      cards.push_back(std::make_unique<CreditCard>(card_on_disk));
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

class PaymentsSyncBridgeUtilTest : public testing::Test {
 public:
  PaymentsSyncBridgeUtilTest() = default;

  PaymentsSyncBridgeUtilTest(const PaymentsSyncBridgeUtilTest&) = delete;
  PaymentsSyncBridgeUtilTest& operator=(const PaymentsSyncBridgeUtilTest&) =
      delete;

  ~PaymentsSyncBridgeUtilTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that PopulateWalletTypesFromSyncData behaves as expected.
TEST_F(PaymentsSyncBridgeUtilTest, PopulateWalletTypesFromSyncData) {
  syncer::EntityChangeList entity_data;
  // Add two credit cards.
  std::string credit_card_id_1 = "credit_card_1";
  std::string credit_card_id_2 = "credit_card_2";
  // Add one IBAN.
  std::string iban_id = "12345678";
  // Add the first card that has its billing address id set to the address's id.
  // No nickname is set.
  sync_pb::AutofillWalletSpecifics wallet_specifics_card1 =
      CreateAutofillWalletSpecificsForCard(
          /*client_tag=*/credit_card_id_1,
          /*billing_address_id=*/"1");
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
          /*client_tag=*/credit_card_id_2,
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
  sync_pb::AutofillWalletSpecifics wallet_specifics_iban =
      CreateAutofillWalletSpecificsForIban(
          /*client_tag=*/iban_id);

  entity_data.push_back(EntityChange::CreateAdd(
      credit_card_id_1,
      SpecificsToEntity(wallet_specifics_card1, /*client_tag=*/"card-card1")));
  entity_data.push_back(EntityChange::CreateAdd(
      credit_card_id_2,
      SpecificsToEntity(wallet_specifics_card2, /*client_tag=*/"card-card2")));
  entity_data.push_back(EntityChange::CreateAdd(
      iban_id,
      SpecificsToEntity(wallet_specifics_iban, /*client_tag=*/"iban")));
  // Add payments customer data.
  entity_data.push_back(EntityChange::CreateAdd(
      "deadbeef",
      SpecificsToEntity(CreateAutofillWalletSpecificsForPaymentsCustomerData(
                            /*client_tag=*/"deadbeef"),
                        /*client_tag=*/"customer-deadbeef")));
  // Add cloud token data.
  entity_data.push_back(EntityChange::CreateAdd(
      "data1", SpecificsToEntity(
                   CreateAutofillWalletSpecificsForCreditCardCloudTokenData(
                       /*client_tag=*/"data1"),
                   /*client_tag=*/"token-data1")));

  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  std::vector<BankAccount> bank_accounts;
  std::vector<CreditCardBenefit> benefits;
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data,
                                  bank_accounts, benefits, payment_instruments);

  ASSERT_EQ(2U, wallet_cards.size());

  EXPECT_EQ("deadbeef", customer_data.back().customer_id);

  EXPECT_EQ("data1", cloud_token_data.back().instrument_token);

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

// Test suite for benefit syncing helpers that takes a bool indicating
// whether benefit syncing should be enabled for testing.
class PaymentsSyncBridgeUtilCardBenefitsTest
    : public PaymentsSyncBridgeUtilTest,
      public testing::WithParamInterface<bool> {
 public:
  PaymentsSyncBridgeUtilCardBenefitsTest() {
    feature_list_.InitWithFeatureState(
        autofill::features::kAutofillEnableCardBenefitsSync,
        IsBenefitsSyncEnabled());
  }

  ~PaymentsSyncBridgeUtilCardBenefitsTest() override = default;

  sync_pb::AutofillWalletSpecifics PrepareCardSpecificForBenefit(
      std::string_view card_tag,
      const int64_t instrument_id) {
    sync_pb::AutofillWalletSpecifics wallet_specifics_card =
        CreateAutofillWalletSpecificsForCard(
            /*client_tag=*/base::StrCat({"credit_card_", card_tag}),
            /*billing_address_id=*/std::string(card_tag));
    wallet_specifics_card.mutable_masked_card()->set_instrument_id(
        instrument_id);
    wallet_specifics_card.mutable_masked_card()->set_product_terms_url(
        base::StrCat({"https://www.example", card_tag, ".com/term"}));
    return wallet_specifics_card;
  }

  void AddBenefitForCard(
      CreditCardBenefit& benefit,
      sync_pb::AutofillWalletSpecifics& wallet_specifics_card) {
    test_api(benefit).SetLinkedCardInstrumentId(
        CreditCardBenefitBase::LinkedCardInstrumentId(
            wallet_specifics_card.mutable_masked_card()->instrument_id()));
    SetAutofillWalletSpecificsFromCardBenefit(benefit, /*enforce_utf8=*/false,
                                              wallet_specifics_card);
  }

  bool IsBenefitsSyncEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Initializes the parameterized test suite with a bool indicating whether
// benefit syncing should be enabled for testing.
INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         PaymentsSyncBridgeUtilCardBenefitsTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& arg) {
                           return arg.param ? "BenefitSyncEnabled"
                                            : "BenefitSyncDisabled";
                         });

// Tests that PopulateWalletTypesFromSyncData behaves as expected for wallet
// card and card benefit.
TEST_P(PaymentsSyncBridgeUtilCardBenefitsTest,
       PopulateWalletTypesFromSyncDataForCreditCardBenefits) {
  // Generate two credit card specifics.
  sync_pb::AutofillWalletSpecifics wallet_specifics_card1 =
      PrepareCardSpecificForBenefit(/*card_tag=*/"1", /*instrument_id=*/1234);
  sync_pb::AutofillWalletSpecifics wallet_specifics_card2 =
      PrepareCardSpecificForBenefit(/*card_tag=*/"2", /*instrument_id=*/5678);

  // Set a time in milliseconds accuracy so that the testing benefits can be
  // generated in milliseconds to match the server format.
  task_environment_.FastForwardBy(base::Milliseconds(1234));

  // Add two credit-card-linked benefits to card 1.
  CreditCardBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  CreditCardBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  AddBenefitForCard(merchant_benefit, wallet_specifics_card1);
  AddBenefitForCard(category_benefit, wallet_specifics_card1);

  // Add one credit-card-linked benefit to card 2.
  CreditCardBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  AddBenefitForCard(flat_rate_benefit, wallet_specifics_card2);

  // Add cards to entity.
  syncer::EntityChangeList entity_data;
  entity_data.push_back(EntityChange::CreateAdd(
      wallet_specifics_card1.mutable_masked_card()->id(),
      SpecificsToEntity(wallet_specifics_card1, /*client_tag=*/"card-card1")));
  entity_data.push_back(EntityChange::CreateAdd(
      wallet_specifics_card2.mutable_masked_card()->id(),
      SpecificsToEntity(wallet_specifics_card2, /*client_tag=*/"card-card2")));

  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  std::vector<BankAccount> bank_accounts;
  std::vector<CreditCardBenefit> benefits;
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data,
                                  bank_accounts, benefits, payment_instruments);

  EXPECT_EQ(2U, wallet_cards.size());

  // Verify that the `product_terms_url` and `card_benefit` are set correctly.
  if (IsBenefitsSyncEnabled()) {
    EXPECT_EQ(
        wallet_cards.front().product_terms_url().spec(),
        wallet_specifics_card1.mutable_masked_card()->product_terms_url());
    EXPECT_EQ(
        wallet_cards.back().product_terms_url().spec(),
        wallet_specifics_card2.mutable_masked_card()->product_terms_url());
    EXPECT_THAT(benefits,
                testing::UnorderedElementsAre(
                    flat_rate_benefit, category_benefit, merchant_benefit));
  } else {
    EXPECT_TRUE(wallet_cards.front().product_terms_url().is_empty());
    EXPECT_TRUE(wallet_cards.back().product_terms_url().is_empty());
    EXPECT_TRUE(benefits.empty());
  }
}

// Verify that the billing address id from the card saved on disk is kept if it
// is a local profile guid.
TEST_F(PaymentsSyncBridgeUtilTest,
       CopyRelevantWalletMetadataAndCvc_KeepLocalAddresses) {
  std::vector<CreditCard> cards_from_local_storage;
  std::vector<CreditCard> wallet_cards;

  // Create a local profile to be used as a billing address.
  AutofillProfile billing_address(AddressCountryCode("US"));

  // Create a card on disk that refers to that local profile as its billing
  // address.
  cards_from_local_storage.emplace_back();
  cards_from_local_storage.back().set_billing_address_id(
      billing_address.guid());

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.emplace_back(cards_from_local_storage.back());
  wallet_cards.back().set_billing_address_id("1234");

  // Setup the TestPaymentsAutofillTable with the `cards_from_local_storage`.
  TestPaymentsAutofillTable table(cards_from_local_storage);

  CopyRelevantWalletMetadataAndCvc(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the wallet card replace its billing address id for the one that
  // was saved on disk.
  EXPECT_EQ(cards_from_local_storage.back().billing_address_id(),
            wallet_cards.back().billing_address_id());
}

// Verify that the billing address id from the card saved on disk is overwritten
// if it does not refer to a local profile.
TEST_F(PaymentsSyncBridgeUtilTest,
       CopyRelevantWalletMetadataAndCvc_OverwriteOtherAddresses) {
  std::string old_billing_id = "1234";
  std::string new_billing_id = "9876";
  std::vector<CreditCard> cards_from_local_storage;
  std::vector<CreditCard> wallet_cards;

  // Create a card on disk that does not refer to a local profile (which have 36
  // chars ids).
  cards_from_local_storage.emplace_back();
  cards_from_local_storage.back().set_billing_address_id(old_billing_id);

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.emplace_back(cards_from_local_storage.back());
  wallet_cards.back().set_billing_address_id(new_billing_id);

  // Setup the TestPaymentsAutofillTable with the `cards_from_local_storage`.
  TestPaymentsAutofillTable table(cards_from_local_storage);

  CopyRelevantWalletMetadataAndCvc(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the local address billing id that was saved on disk did not
  // replace the new one.
  EXPECT_EQ(new_billing_id, wallet_cards.back().billing_address_id());
}

// Verify that the use stats on disk are kept when server cards are synced.
TEST_F(PaymentsSyncBridgeUtilTest,
       CopyRelevantWalletMetadataAndCvc_KeepUseStats) {
  base::Time disk_time = base::Time::FromSecondsSinceUnixEpoch(10);
  task_environment_.FastForwardBy(base::Seconds(25));

  std::vector<CreditCard> cards_from_local_storage;
  std::vector<CreditCard> wallet_cards;

  // Create a card on disk with specific use stats.
  cards_from_local_storage.emplace_back();
  cards_from_local_storage.back().set_use_count(3U);
  cards_from_local_storage.back().set_use_date(disk_time);

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.emplace_back();
  wallet_cards.back().set_use_count(10U);

  // Setup the TestPaymentsAutofillTable with the `cards_from_local_storage`.
  TestPaymentsAutofillTable table(cards_from_local_storage);

  CopyRelevantWalletMetadataAndCvc(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the use stats from disk were kept
  EXPECT_EQ(3U, wallet_cards.back().use_count());
  EXPECT_EQ(disk_time, wallet_cards.back().use_date());
}

// Verify that the credential data on disk are kept when server cards are
// synced.
TEST_F(PaymentsSyncBridgeUtilTest,
       CopyRelevantWalletMetadataAndCvc_KeepCredentialData) {
  std::vector<CreditCard> cards_from_local_storage;
  std::vector<CreditCard> wallet_cards;

  // Create a card on disk with specific use stats.
  cards_from_local_storage.emplace_back();
  cards_from_local_storage.back().set_cvc(u"123");

  // Create a card pulled from wallet with the same id, but with an empty CVC.
  wallet_cards.emplace_back();

  // Setup the TestPaymentsAutofillTable with the `cards_from_local_storage`.
  TestPaymentsAutofillTable table(cards_from_local_storage);

  CopyRelevantWalletMetadataAndCvc(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Verify the wallet credential (CVC) data.
  EXPECT_EQ(u"123", wallet_cards.back().cvc());
}

// Test to ensure the general-purpose fields from an AutofillOfferData are
// correctly converted to an AutofillOfferSpecifics.
TEST_F(PaymentsSyncBridgeUtilTest, OfferSpecificsFromOfferData) {
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
TEST_F(PaymentsSyncBridgeUtilTest, OfferSpecificsFromCardLinkedOfferData) {
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
TEST_F(PaymentsSyncBridgeUtilTest, OfferSpecificsFromPromoCodeOfferData) {
  sync_pb::AutofillOfferSpecifics offer_specifics;
  AutofillOfferData offer_data = test::GetPromoCodeOfferData();
  SetAutofillOfferSpecificsFromOfferData(offer_data, &offer_specifics);

  EXPECT_EQ(offer_specifics.promo_code_offer_data().promo_code(),
            offer_data.GetPromoCode());
}

// Ensures that the ShouldResetAutofillWalletData function works correctly, if
// the two given data sets have the same size.
TEST_F(PaymentsSyncBridgeUtilTest,
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

  new_offer_data.front().SetOfferIdForTesting(
      new_offer_data.at(0).GetOfferId() + 456);
  EXPECT_TRUE(AreAnyItemsDifferent(old_offer_data, new_offer_data));

  CreditCardBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  CreditCardBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  std::vector<CreditCardBenefit> old_card_benefits = {flat_rate_benefit,
                                                      merchant_benefit};
  std::vector<CreditCardBenefit> new_card_benefits = {merchant_benefit,
                                                      flat_rate_benefit};
  EXPECT_FALSE(AreAnyItemsDifferent(old_card_benefits, new_card_benefits));

  test_api(new_card_benefits.front())
      .SetBenefitId(CreditCardBenefitBase::BenefitId("DifferentId"));
  EXPECT_TRUE(AreAnyItemsDifferent(old_card_benefits, new_card_benefits));
}

// Ensures that the ShouldResetAutofillWalletData function works correctly, if
// the two given data sets have different size.
TEST_F(PaymentsSyncBridgeUtilTest,
       ShouldResetAutofillWalletData_DifferentDataSetSize) {
  std::vector<std::unique_ptr<AutofillOfferData>> old_offer_data;
  std::vector<AutofillOfferData> new_offer_data;

  AutofillOfferData data1 = test::GetCardLinkedOfferData1();
  AutofillOfferData data2 = test::GetCardLinkedOfferData2();
  old_offer_data.push_back(std::make_unique<AutofillOfferData>(data1));
  new_offer_data.push_back(data2);
  new_offer_data.push_back(data1);
  EXPECT_TRUE(AreAnyItemsDifferent(old_offer_data, new_offer_data));

  std::vector<CreditCardBenefit> old_card_benefits = {
      test::GetActiveCreditCardMerchantBenefit(),
      test::GetActiveCreditCardCategoryBenefit()};
  std::vector<CreditCardBenefit> new_card_benefits = {
      test::GetActiveCreditCardMerchantBenefit()};
  EXPECT_TRUE(AreAnyItemsDifferent(old_card_benefits, new_card_benefits));
}

// Ensures that function IsOfferSpecificsValid is working correctly.
TEST_F(PaymentsSyncBridgeUtilTest, IsOfferSpecificsValid) {
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
TEST_F(PaymentsSyncBridgeUtilTest, WalletUsageSpecificsFromWalletUsageData) {
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
TEST_F(PaymentsSyncBridgeUtilTest, VirtualCardUsageDataFromUsageSpecifics) {
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
TEST_F(PaymentsSyncBridgeUtilTest,
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

TEST_F(PaymentsSyncBridgeUtilTest, AutofillWalletStructDataFromUsageSpecifics) {
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

// Test to ensure that CreditCardBenefits can be correctly converted to
// AutofillWalletSpecifics.
TEST_F(PaymentsSyncBridgeUtilTest, SetAutofillWalletSpecificsFromCardBenefit) {
  // Get one credit-card-linked benefit for each type.
  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(category_benefit).SetStartTime(base::Time::Min());
  test_api(flat_rate_benefit).SetExpiryTime(base::Time::Max());

  // Add above benefits to card specifics.
  sync_pb::AutofillWalletSpecifics wallet_specifics_card =
      CreateAutofillWalletSpecificsForCard(/*client_tag=*/"credit_card");
  SetAutofillWalletSpecificsFromCardBenefit(
      merchant_benefit, /*enforce_utf8=*/false, wallet_specifics_card);
  SetAutofillWalletSpecificsFromCardBenefit(
      category_benefit, /*enforce_utf8=*/false, wallet_specifics_card);
  SetAutofillWalletSpecificsFromCardBenefit(
      flat_rate_benefit, /*enforce_utf8=*/false, wallet_specifics_card);

  // Check type specific benefit fields are set correctly.
  for (const auto& benefit_specifics :
       wallet_specifics_card.masked_card().card_benefit()) {
    std::optional<CreditCardBenefit> target_benefit;

    if (benefit_specifics.has_flat_rate_benefit()) {
      target_benefit = flat_rate_benefit;
    } else if (benefit_specifics.has_category_benefit()) {
      // Check category benefit specific field is set correctly.
      EXPECT_EQ(
          base::to_underlying(
              benefit_specifics.category_benefit().category_benefit_type()),
          base::to_underlying(category_benefit.benefit_category()));

      target_benefit = category_benefit;
    } else {
      EXPECT_TRUE(benefit_specifics.has_merchant_benefit());

      // Check merchant benefit specific field is set correctly.
      const base::flat_set<url::Origin>& benefit_merchant_domains =
          merchant_benefit.merchant_domains();
      EXPECT_TRUE(benefit_specifics.merchant_benefit().merchant_domain_size() ==
                  static_cast<int>(benefit_merchant_domains.size()));
      for (const std::string& specifics_merchant_domain :
           benefit_specifics.merchant_benefit().merchant_domain()) {
        EXPECT_TRUE(benefit_merchant_domains.contains(
            url::Origin::Create(GURL(specifics_merchant_domain))));
      }

      target_benefit = merchant_benefit;
    }

    // Check benefit common fields are set correctly.
    CreditCardBenefitBase& benefit_base = absl::visit(
        [](auto& benefit) -> CreditCardBenefitBase& { return benefit; },
        *target_benefit);

    EXPECT_EQ(benefit_specifics.benefit_id(),
              benefit_base.benefit_id().value());

    // Specifics with empty start time means the benefit has no start time
    // (always started). The client benefit start time will be set to min
    // time with such specifics.
    // So when the benefit start time is min time, the specifics start time
    // should have no value.
    if (benefit_base.start_time().is_min()) {
      EXPECT_FALSE(benefit_specifics.has_start_time_unix_epoch_milliseconds());
    } else {
      EXPECT_EQ(benefit_specifics.start_time_unix_epoch_milliseconds(),
                benefit_base.start_time().InMillisecondsSinceUnixEpoch());
    }

    // Specifics with empty end time means the benefit has no expiry time
    // (never expires). The client benefit expiry time will be set to max
    // time with such specifics.
    // So when the benefit expiry time is max time, the specifics end time
    // should have no value.
    if (benefit_base.expiry_time().is_max()) {
      EXPECT_FALSE(benefit_specifics.has_end_time_unix_epoch_milliseconds());
    } else {
      EXPECT_EQ(benefit_specifics.end_time_unix_epoch_milliseconds(),
                benefit_base.expiry_time().InMillisecondsSinceUnixEpoch());
    }

    EXPECT_EQ(base::UTF8ToUTF16(benefit_specifics.benefit_description()),
              benefit_base.benefit_description());
  }
}

// Round trip test to ensure that WalletCredential struct data for CVV storage
// is correctly converted to AutofillWalletCredentialSpecifics and then from
// the converted AutofillWalletCredentialSpecifics to WalletCredential struct
// data. In the end we compare the original and the converted struct data.
TEST_F(PaymentsSyncBridgeUtilTest,
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

#if BUILDFLAG(IS_ANDROID)
// Tests that PopulateWalletTypesFromSyncData populates BankAccounts.
TEST_F(PaymentsSyncBridgeUtilTest, PopulateBankAccountFromSyncData) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  syncer::EntityChangeList entity_data;
  std::string bank_account_id = "payment_instrument:123545";
  sync_pb::AutofillWalletSpecifics payment_instrument_bank_account_specifics =
      CreateAutofillWalletSpecificsForBankAccount(
          /*client_tag=*/bank_account_id, /*nickname=*/"Pix bank account",
          /*display_icon_url=*/GURL("http://www.google.com"),
          /*bank_name=*/"ABC Bank",
          /*account_number_suffix=*/"1234",
          sync_pb::BankAccountDetails_AccountType_CHECKING);
  entity_data.push_back(EntityChange::CreateAdd(
      bank_account_id,
      SpecificsToEntity(payment_instrument_bank_account_specifics,
                        /*client_tag=*/"bank_account")));
  BankAccount expected_bank_account(
      /*instrument_id=*/123545, /*nickname=*/u"Pix bank account",
      /*display_icon_url=*/GURL("http://www.google.com"),
      /*bank_name=*/u"ABC Bank",
      /*account_number_suffix=*/u"1234", BankAccount::AccountType::kChecking);

  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  std::vector<BankAccount> bank_accounts;
  std::vector<CreditCardBenefit> benefits;
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data,
                                  bank_accounts, benefits, payment_instruments);

  ASSERT_EQ(1u, bank_accounts.size());
  EXPECT_EQ(expected_bank_account, bank_accounts.at(0));
}

// Tests that PopulateWalletTypesFromSyncData does not BankAccounts if Pix
// experiment flag is disabled.
TEST_F(PaymentsSyncBridgeUtilTest,
       PopulateBankAccountFromSyncDataExperimentOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  syncer::EntityChangeList entity_data;
  std::string bank_account_id = "payment_instrument:123545";
  sync_pb::AutofillWalletSpecifics payment_instrument_bank_account_specifics =
      CreateAutofillWalletSpecificsForBankAccount(
          /*client_tag=*/bank_account_id, /*nickname=*/"Pix bank account",
          /*display_icon_url=*/GURL("http://www.google.com"),
          /*bank_name=*/"ABC Bank",
          /*account_number_suffix=*/"1234",
          sync_pb::BankAccountDetails_AccountType_CHECKING);
  entity_data.push_back(EntityChange::CreateAdd(
      bank_account_id,
      SpecificsToEntity(payment_instrument_bank_account_specifics,
                        /*client_tag=*/"bank_account")));

  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  std::vector<BankAccount> bank_accounts;
  std::vector<CreditCardBenefit> benefits;
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data,
                                  bank_accounts, benefits, payment_instruments);

  EXPECT_EQ(0u, bank_accounts.size());
}

TEST_F(PaymentsSyncBridgeUtilTest, BankAccountFromWalletSpecifics) {
  sync_pb::AutofillWalletSpecifics payment_instrument_bank_account_specifics =
      CreateAutofillWalletSpecificsForBankAccount(
          /*client_tag=*/"payment_instrument:123545",
          /*nickname=*/"Pix bank account",
          /*display_icon_url=*/GURL("http://www.google.com"),
          /*bank_name=*/"ABC Bank",
          /*account_number_suffix=*/"1234",
          sync_pb::BankAccountDetails_AccountType_CHECKING);
  BankAccount expected_bank_account(
      /*instrument_id=*/123545, /*nickname=*/u"Pix bank account",
      /*display_icon_url=*/GURL("http://www.google.com"),
      /*bank_name=*/u"ABC Bank",
      /*account_number_suffix=*/u"1234", BankAccount::AccountType::kChecking);

  EXPECT_EQ(
      expected_bank_account,
      BankAccountFromWalletSpecifics(
          payment_instrument_bank_account_specifics.payment_instrument()));
}

// Tests that PopulateWalletTypesFromSyncData populates PaymentInstruments for
// eWallet accounts.
TEST_F(PaymentsSyncBridgeUtilTest,
       PopulatePaymentInstrumentsFromSyncData_EwalletAccounts) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  syncer::EntityChangeList entity_data;
  std::string ewallet_account_id = "payment_instrument:123545";
  sync_pb::AutofillWalletSpecifics
      payment_instrument_ewallet_account_specifics =
          CreateAutofillWalletSpecificsForEwalletAccount(
              /*client_tag=*/ewallet_account_id, /*nickname=*/"eWallet account",
              /*display_icon_url=*/GURL("http://www.google.com"),
              /*ewallet_name=*/"ABC Pay",
              /*account_display_name=*/"1234",
              /*is_fido_enrolled=*/false);
  entity_data.push_back(EntityChange::CreateAdd(
      ewallet_account_id,
      SpecificsToEntity(payment_instrument_ewallet_account_specifics,
                        /*client_tag=*/"ewallet_account")));

  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  std::vector<BankAccount> bank_accounts;
  std::vector<CreditCardBenefit> benefits;
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data,
                                  bank_accounts, benefits, payment_instruments);

  ASSERT_EQ(1u, payment_instruments.size());
  sync_pb::PaymentInstrument payment_instrument = payment_instruments.at(0);
  EXPECT_EQ(payment_instrument_ewallet_account_specifics.payment_instrument()
                .instrument_id(),
            payment_instrument.instrument_id());
  EXPECT_EQ(payment_instrument_ewallet_account_specifics.payment_instrument()
                .nickname(),
            payment_instrument.nickname());
  EXPECT_EQ(payment_instrument_ewallet_account_specifics.payment_instrument()
                .display_icon_url(),
            payment_instrument.display_icon_url());
  EXPECT_EQ(payment_instrument_ewallet_account_specifics.payment_instrument()
                .supported_rails()
                .at(0),
            payment_instrument.supported_rails().at(0));
  EXPECT_EQ(payment_instrument_ewallet_account_specifics.payment_instrument()
                .ewallet_details()
                .ewallet_name(),
            payment_instrument.ewallet_details().ewallet_name());
  EXPECT_EQ(payment_instrument_ewallet_account_specifics.payment_instrument()
                .ewallet_details()
                .account_display_name(),
            payment_instrument.ewallet_details().account_display_name());
  EXPECT_EQ(
      payment_instrument_ewallet_account_specifics.payment_instrument()
          .ewallet_details()
          .supported_payment_link_uris()
          .at(0),
      payment_instrument.ewallet_details().supported_payment_link_uris().at(0));
  EXPECT_EQ(payment_instrument_ewallet_account_specifics.payment_instrument()
                .device_details()
                .is_fido_enrolled(),
            payment_instrument.device_details().is_fido_enrolled());
}

// Tests that PopulateWalletTypesFromSyncData does not populate
// PaymentInstruments for eWallet accounts if eWallet sync experiment flag is
// disabled.
TEST_F(PaymentsSyncBridgeUtilTest,
       PopulatePaymentInstrumentsFromSyncDataExperimentOff_EwalletAccounts) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillSyncEwalletAccounts);
  syncer::EntityChangeList entity_data;
  std::string ewallet_account_id = "payment_instrument:123545";
  sync_pb::AutofillWalletSpecifics
      payment_instrument_ewallet_account_specifics =
          CreateAutofillWalletSpecificsForEwalletAccount(
              /*client_tag=*/ewallet_account_id, /*nickname=*/"eWallet account",
              /*display_icon_url=*/GURL("http://www.google.com"),
              /*ewallet_name=*/"ABC Pay",
              /*account_display_name=*/"1234",
              /*is_fido_enrolled=*/false);
  entity_data.push_back(EntityChange::CreateAdd(
      ewallet_account_id,
      SpecificsToEntity(payment_instrument_ewallet_account_specifics,
                        /*client_tag=*/"ewallet_account")));

  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  std::vector<BankAccount> bank_accounts;
  std::vector<CreditCardBenefit> benefits;
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data,
                                  bank_accounts, benefits, payment_instruments);

  EXPECT_EQ(0u, payment_instruments.size());
}
#endif  // BUILDFLAG(IS_ANDROID)

struct WalletCardTypeMapping {
  sync_pb::WalletMaskedCreditCard_WalletCardType wallet_card_type;
  const char* const card_network;
};

class PaymentsSyncBridgeUtilTest_WalletCardMapping
    : public ::testing::TestWithParam<WalletCardTypeMapping> {};

// Test to verify the correct mapping of CardType to the card network.
TEST_P(PaymentsSyncBridgeUtilTest_WalletCardMapping,
       VerifyCardTypeMappingFromCardNetwork) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableVerveCardSupport);
  auto test_case = GetParam();

  syncer::EntityChangeList entity_data;
  // Add a credit card.
  std::string credit_card_id = "credit_card_1";
  // Add the first card. No nickname is set.
  sync_pb::AutofillWalletSpecifics wallet_specifics_card =
      CreateAutofillWalletSpecificsForCard(
          /*client_tag=*/credit_card_id,
          /*billing_address_id=*/"1");
  wallet_specifics_card.mutable_masked_card()->set_type(
      test_case.wallet_card_type);

  entity_data.push_back(EntityChange::CreateAdd(
      credit_card_id,
      SpecificsToEntity(wallet_specifics_card, /*client_tag=*/"card-card1")));

  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  std::vector<BankAccount> bank_accounts;
  std::vector<CreditCardBenefit> benefits;
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data,
                                  bank_accounts, benefits, payment_instruments);

  ASSERT_EQ(1U, wallet_cards.size());
  EXPECT_EQ(test_case.card_network, wallet_cards.front().network());
}

// Test to verify the correct mapping of the card network to CardType.
TEST_P(PaymentsSyncBridgeUtilTest_WalletCardMapping,
       VerifyCardNetworkMappingFromCardType) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableVerveCardSupport);
  auto test_case = GetParam();

  CreditCard credit_card = test::GetMaskedServerCard();
  credit_card.SetNetworkForMaskedCard(test_case.card_network);

  sync_pb::AutofillWalletSpecifics wallet_specifics;
  SetAutofillWalletSpecificsFromServerCard(credit_card, &wallet_specifics);

  EXPECT_EQ(test_case.wallet_card_type,
            wallet_specifics.mutable_masked_card()->type());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PaymentsSyncBridgeUtilTest_WalletCardMapping,
    testing::Values(
        WalletCardTypeMapping{sync_pb::WalletMaskedCreditCard::AMEX,
                              autofill::kAmericanExpressCard},
        WalletCardTypeMapping{sync_pb::WalletMaskedCreditCard::DISCOVER,
                              autofill::kDiscoverCard},
        WalletCardTypeMapping{sync_pb::WalletMaskedCreditCard::ELO,
                              autofill::kEloCard},
        WalletCardTypeMapping{sync_pb::WalletMaskedCreditCard::JCB,
                              autofill::kJCBCard},
        WalletCardTypeMapping{sync_pb::WalletMaskedCreditCard::MASTER_CARD,
                              autofill::kMasterCard},
        WalletCardTypeMapping{sync_pb::WalletMaskedCreditCard::UNIONPAY,
                              autofill::kUnionPay},
        WalletCardTypeMapping{sync_pb::WalletMaskedCreditCard::VERVE,
                              autofill::kVerveCard},
        WalletCardTypeMapping{sync_pb::WalletMaskedCreditCard::VISA,
                              autofill::kVisaCard}));

// These two tests verify the same case as
// `PaymentsSyncBridgeUtilTest_WalletCardMapping` but with the added caveat of
// checking the Verve conversion values with `kAutofillEnableVerveCardSupport`
// flag off.
TEST_F(PaymentsSyncBridgeUtilTest,
       VerifyCardNetworkMappingFromCardType_ForVerve_WithFlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableVerveCardSupport);

  CreditCard credit_card = test::GetMaskedServerCard();
  credit_card.SetNetworkForMaskedCard(autofill::kVerveCard);

  sync_pb::AutofillWalletSpecifics wallet_specifics;
  SetAutofillWalletSpecificsFromServerCard(credit_card, &wallet_specifics);

  // With the flag off, the card type is UNKNOWN instead of VERVE.
  EXPECT_EQ(sync_pb::WalletMaskedCreditCard::UNKNOWN,
            wallet_specifics.mutable_masked_card()->type());
}

TEST_F(PaymentsSyncBridgeUtilTest,
       VerifyCardTypeMappingFromCardNetwork_ForVerve_WithFlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableVerveCardSupport);

  syncer::EntityChangeList entity_data;
  // Add a credit card.
  std::string credit_card_id = "credit_card_1";
  // Add the first card. No nickname is set.
  sync_pb::AutofillWalletSpecifics wallet_specifics_card =
      CreateAutofillWalletSpecificsForCard(
          /*client_tag=*/credit_card_id,
          /*billing_address_id=*/"1");
  wallet_specifics_card.mutable_masked_card()->set_type(
      sync_pb::WalletMaskedCreditCard::VERVE);

  entity_data.push_back(EntityChange::CreateAdd(
      credit_card_id,
      SpecificsToEntity(wallet_specifics_card, /*client_tag=*/"card-card1")));

  std::vector<CreditCard> wallet_cards;
  std::vector<Iban> wallet_ibans;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  std::vector<BankAccount> bank_accounts;
  std::vector<CreditCardBenefit> benefits;
  std::vector<sync_pb::PaymentInstrument> payment_instruments;
  PopulateWalletTypesFromSyncData(entity_data, wallet_cards, wallet_ibans,
                                  customer_data, cloud_token_data,
                                  bank_accounts, benefits, payment_instruments);

  ASSERT_EQ(1U, wallet_cards.size());
  // With the flag off, the card network is `kGenericCard` instead of
  // `kVerveCard`.
  EXPECT_EQ(autofill::kGenericCard, wallet_cards.front().network());
}

TEST_F(PaymentsSyncBridgeUtilTest,
       AreAnyPaymentInstrumentsDifferent_ReturnFalseForSameData) {
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/2345);
  std::vector<sync_pb::PaymentInstrument> payment_instruments_1 = {
      payment_instrument_1, payment_instrument_2};

  sync_pb::PaymentInstrument payment_instrument_3 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  sync_pb::PaymentInstrument payment_instrument_4 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/2345);
  std::vector<sync_pb::PaymentInstrument> payment_instruments_2 = {
      payment_instrument_3, payment_instrument_4};

  EXPECT_FALSE(
      AreAnyItemsDifferent(payment_instruments_1, payment_instruments_2));
}

TEST_F(PaymentsSyncBridgeUtilTest,
       AreAnyPaymentInstrumentsDifferent_ReturnTrueForDifferentDataSize) {
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/2345);
  std::vector<sync_pb::PaymentInstrument> payment_instruments_1 = {
      payment_instrument_1, payment_instrument_2};

  sync_pb::PaymentInstrument payment_instrument_3 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  std::vector<sync_pb::PaymentInstrument> payment_instruments_2 = {
      payment_instrument_3};

  EXPECT_TRUE(
      AreAnyItemsDifferent(payment_instruments_1, payment_instruments_2));
}

TEST_F(PaymentsSyncBridgeUtilTest,
       AreAnyPaymentInstrumentsDifferent_ReturnTrueForDifferentInstrumentId) {
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  std::vector<sync_pb::PaymentInstrument> payment_instruments_1 = {
      payment_instrument_1};

  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/2345);
  std::vector<sync_pb::PaymentInstrument> payment_instruments_2 = {
      payment_instrument_2};

  EXPECT_TRUE(
      AreAnyItemsDifferent(payment_instruments_1, payment_instruments_2));
}

TEST_F(PaymentsSyncBridgeUtilTest,
       AreAnyPaymentInstrumentsDifferent_ReturnTrueForDifferentEwalletDetails) {
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  std::vector<sync_pb::PaymentInstrument> payment_instruments_1 = {
      payment_instrument_1};

  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(/*instrument_id=*/1234);
  sync_pb::EwalletDetails* ewallet_2 =
      payment_instrument_2.mutable_ewallet_details();
  ewallet_2->set_ewallet_name("different_ewallet_name");
  std::vector<sync_pb::PaymentInstrument> payment_instruments_2 = {
      payment_instrument_2};

  EXPECT_TRUE(
      AreAnyItemsDifferent(payment_instruments_1, payment_instruments_2));
}

}  // namespace
}  // namespace autofill
