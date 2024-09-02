// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/migrate_cards_request.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/test/autofill_payments_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace autofill::payments {
namespace {

std::unique_ptr<MigrateCardsRequest> CreateMigrateCardsRequest(
    std::vector<MigratableCreditCard>* migratable_credit_cards,
    bool has_cardholder_name,
    bool set_nickname_for_first_card = false) {
  CHECK(migratable_credit_cards);
  CHECK(migratable_credit_cards->empty());

  PaymentsNetworkInterface::MigrationRequestDetails request_details;
  request_details.context_token = u"context token";
  request_details.risk_data = "some risk data";
  request_details.app_locale = "language-LOCALE";

  CreditCard card1 = test::GetCreditCard();
  if (set_nickname_for_first_card) {
    card1.SetNickname(u"grocery");
  }
  CreditCard card2 = test::GetCreditCard2();
  if (!has_cardholder_name) {
    card1.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
    card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
  }
  migratable_credit_cards->emplace_back(card1);
  migratable_credit_cards->emplace_back(card2);

  return std::make_unique<MigrateCardsRequest>(
      request_details, *migratable_credit_cards,
      /*full_sync_enabled=*/true, base::DoNothing());
}

TEST(MigrateCardsRequestTest, MigrationRequestIncludesUniqueId) {
  std::vector<MigratableCreditCard> migratable_credit_cards;
  std::unique_ptr<MigrateCardsRequest> request = CreateMigrateCardsRequest(
      &migratable_credit_cards, /*has_cardholder_name=*/true);

  // Verify that the unique id was included in the request.
  EXPECT_TRUE(request->GetRequestContent().find("unique_id") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find(
                  migratable_credit_cards[0].credit_card().guid()) !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find(
                  migratable_credit_cards[1].credit_card().guid()) !=
              std::string::npos);
}

TEST(MigrateCardsRequestTest, MigrationRequestIncludesEncryptedPan) {
  std::vector<MigratableCreditCard> migratable_credit_cards;
  std::unique_ptr<MigrateCardsRequest> request = CreateMigrateCardsRequest(
      &migratable_credit_cards, /*has_cardholder_name=*/true);

  // Verify that the encrypted_pan and s7e_1_pan parameters were included
  // in the request.
  EXPECT_TRUE(request->GetRequestContent().find("encrypted_pan") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("__param:s7e_1_pan0") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find(
                  "&s7e_1_pan0=4111111111111111") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("__param:s7e_1_pan1") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find(
                  "&s7e_1_pan1=378282246310005") != std::string::npos);
}

TEST(MigrateCardsRequestTest,
     MigrationRequestIncludesCardholderNameWhenItExists) {
  std::vector<MigratableCreditCard> migratable_credit_cards;
  std::unique_ptr<MigrateCardsRequest> request = CreateMigrateCardsRequest(
      &migratable_credit_cards, /*has_cardholder_name=*/true);

  EXPECT_TRUE(!request->GetRequestContent().empty());
  // Verify that the cardholder name is sent if it exists.
  EXPECT_TRUE(request->GetRequestContent().find("cardholder_name") !=
              std::string::npos);
}

TEST(MigrateCardsRequestTest,
     MigrationRequestExcludesCardholderNameWhenItDoesNotExist) {
  std::vector<MigratableCreditCard> migratable_credit_cards;
  std::unique_ptr<MigrateCardsRequest> request = CreateMigrateCardsRequest(
      &migratable_credit_cards, /*has_cardholder_name=*/false);

  EXPECT_TRUE(!request->GetRequestContent().empty());
  // Verify that the cardholder name is not sent if it doesn't exist.
  EXPECT_TRUE(request->GetRequestContent().find("cardholder_name") ==
              std::string::npos);
}

TEST(MigrateCardsRequestTest, MigrationRequestIncludesChromeUserContext) {
  std::vector<MigratableCreditCard> migratable_credit_cards;
  std::unique_ptr<MigrateCardsRequest> request = CreateMigrateCardsRequest(
      &migratable_credit_cards, /*has_cardholder_name=*/true);

  // ChromeUserContext was set.
  EXPECT_TRUE(request->GetRequestContent().find("chrome_user_context") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("full_sync_enabled") !=
              std::string::npos);
}

TEST(MigrateCardsRequestTest, MigrationRequestIncludesCardNickname) {
  std::vector<MigratableCreditCard> migratable_credit_cards;
  std::unique_ptr<MigrateCardsRequest> request = CreateMigrateCardsRequest(
      &migratable_credit_cards, /*has_cardholder_name=*/true,
      /*set_nickname_for_first_card=*/true);

  // Nickname was set for the first card.
  std::size_t pos = request->GetRequestContent().find("nickname");
  EXPECT_TRUE(pos != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find(base::UTF16ToUTF8(
                  migratable_credit_cards[0].credit_card().nickname())) !=
              std::string::npos);

  // Nickname was not set for the second card.
  EXPECT_FALSE(request->GetRequestContent().find("nickname", pos + 1) !=
               std::string::npos);
}

}  // namespace
}  // namespace autofill::payments
