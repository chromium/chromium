// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/grit/components_scaled_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

MATCHER_P(HasLegalMessageLineText, text, "A LegalMessageLine that has text.") {
  return base::UTF16ToUTF8(arg.text()) == text;
};

namespace {

// The different representations of a year needed by tests here.
struct Year {
  int integer;
  std::string string;
  std::u16string u16string;
  std::string last2_string;
};

Year SetupNextYear() {
  std::string next_year = test::NextYear();
  int integer;
  EXPECT_TRUE(base::StringToInt(next_year, &integer));
  return Year{
      .integer = integer,
      .string = next_year,
      .u16string = base::ASCIIToUTF16(next_year),
      .last2_string = next_year.substr(next_year.length() - 2, 2),
  };
}

}  // namespace

// Tests that CreateForLocalSave() sets all properties.
TEST(AutofillSaveCardUiInfoTest, CreateForLocalSaveSetsProperties) {
  const Year next_year = SetupNextYear();
  CreditCard card = test::GetCreditCard();
  card.SetNickname(u"My Card");
  card.SetNumber(u"378282246310005");  // This number sets the card network.
  EXPECT_EQ(card.network(), kAmericanExpressCard);  // Self test to ensure the
                                                    // number above is for the
                                                    // intended network.
  card.SetExpirationMonth(3);
  card.SetExpirationYear(next_year.integer);
  card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Chromium Dev");

  auto ui_info = AutofillSaveCardUiInfo::CreateForLocalSave(
      /*options=*/{}, card);

  EXPECT_EQ(ui_info.is_for_upload, false);
  EXPECT_EQ(ui_info.logo_icon_id, IDR_INFOBAR_AUTOFILL_CC);
  EXPECT_EQ(ui_info.issuer_icon_id, IDR_AUTOFILL_CC_AMEX);
  EXPECT_THAT(ui_info.legal_message_lines, testing::ElementsAre());
  EXPECT_EQ(ui_info.card_label, card.NicknameAndLastFourDigitsForTesting());
  EXPECT_THAT(base::UTF16ToUTF8(ui_info.card_sub_label),
              testing::AllOf(testing::HasSubstr(next_year.last2_string),
                             testing::HasSubstr("03")));
  EXPECT_EQ(ui_info.card_last_four_digits, u"0005");
  EXPECT_EQ(ui_info.cardholder_name, u"Chromium Dev");
  EXPECT_EQ(ui_info.expiration_date_month, u"03");
  EXPECT_EQ(ui_info.expiration_date_year, next_year.u16string);
  EXPECT_EQ(ui_info.card_description,
            u"My Card, Amex, 0005, expires 03/" + next_year.u16string);
  EXPECT_EQ(ui_info.displayed_target_account_email, u"");
  EXPECT_TRUE(ui_info.displayed_target_account_avatar.IsEmpty());
  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
  EXPECT_EQ(ui_info.cancel_text, l10n_util::GetStringUTF16(
                                     IDS_AUTOFILL_NO_THANKS_MOBILE_LOCAL_SAVE));
  EXPECT_EQ(ui_info.description_text, std::u16string());
  EXPECT_EQ(ui_info.is_google_pay_branding_enabled, false);
}

// Tests that CreateForUploadSave() sets properties where no branched logic is
// needed.
TEST(AutofillSaveCardUiInfoTest, CreateForUploadSaveSetsProperties) {
  const Year next_year = SetupNextYear();
  CreditCard card = test::GetMaskedServerCard();
  card.SetNickname(u"My Card");
  card.SetNumber(u"4444333322221111");
  card.SetNetworkForMaskedCard(kVisaCard);
  card.SetExpirationMonth(3);
  card.SetExpirationYear(next_year.integer);
  card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Chromium Dev");
  LegalMessageLines legal_message_lines(
      {TestLegalMessageLine("example message")});
  AccountInfo account_info;
  account_info.account_image = gfx::test::CreateImage(11, 17);
  account_info.email = "example email";

  auto ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
      /*options=*/{}, card, legal_message_lines, account_info,
      /*is_google_pay_branding_enabled=*/false);

  EXPECT_EQ(ui_info.is_for_upload, true);
  EXPECT_EQ(ui_info.logo_icon_id, IDR_INFOBAR_AUTOFILL_CC);
  EXPECT_EQ(ui_info.issuer_icon_id, IDR_AUTOFILL_CC_VISA);
  EXPECT_THAT(ui_info.legal_message_lines,
              testing::ElementsAre(HasLegalMessageLineText("example message")));
  EXPECT_THAT(base::UTF16ToUTF8(ui_info.card_label),
              testing::AllOf(testing::HasSubstr("My Card"),
                             testing::HasSubstr("1111")));
  EXPECT_THAT(base::UTF16ToUTF8(ui_info.card_sub_label),
              testing::AllOf(testing::HasSubstr(next_year.last2_string),
                             testing::HasSubstr("03")));
  EXPECT_EQ(ui_info.card_last_four_digits, u"1111");
  EXPECT_EQ(ui_info.cardholder_name, u"Chromium Dev");
  EXPECT_EQ(ui_info.expiration_date_month, u"03");
  EXPECT_EQ(ui_info.expiration_date_year, next_year.u16string);
  EXPECT_EQ(ui_info.card_description,
            u"My Card, Visa, 1111, expires 03/" + next_year.u16string);
  EXPECT_EQ(ui_info.displayed_target_account_email, u"example email");
  EXPECT_EQ(ui_info.displayed_target_account_avatar.Size(), gfx::Size(11, 17));
  EXPECT_EQ(
      ui_info.title_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD));
  EXPECT_EQ(
      ui_info.cancel_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_NO_THANKS_MOBILE_UPLOAD_SAVE));
  EXPECT_EQ(ui_info.description_text, std::u16string());
  EXPECT_EQ(ui_info.is_google_pay_branding_enabled, false);
}

TEST(AutofillSaveCardUiInfoTest,
     CreateForUploadSaveSetsCardDescriptionWithoutNickname) {
  const Year next_year = SetupNextYear();
  CreditCard card = test::GetMaskedServerCard();
  card.SetNumber(u"4444333322221111");
  card.SetNetworkForMaskedCard(kVisaCard);
  card.SetExpirationMonth(3);
  card.SetExpirationYear(next_year.integer);

  auto ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
      /*options=*/{}, card, LegalMessageLines(), AccountInfo(),
      /*is_google_pay_branding_enabled=*/false);

  EXPECT_EQ(ui_info.card_description,
            u"Visa, 1111, expires 03/" + next_year.u16string);
}

// Tests that CreateForUploadSave() sets properties that change under
// GoogleBranding such as
//   * The logo icon,
//   * the title text,
//   * the cancel text,
//   * the description text, and
//   * the is_google_pay_branding_enabled property.
TEST(AutofillSaveCardUiInfoTest,
     CreateForUploadSaveSetsGoogleBrandedProperties) {
  CreditCard card = test::GetMaskedServerCard();

  auto ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
      /*options=*/{}, card, LegalMessageLines(), AccountInfo(),
      /*is_google_pay_branding_enabled=*/true);

  EXPECT_EQ(ui_info.logo_icon_id, IDR_AUTOFILL_GOOGLE_PAY);
  EXPECT_EQ(ui_info.title_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3));
  EXPECT_EQ(
      ui_info.cancel_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_NO_THANKS_MOBILE_UPLOAD_SAVE));
  EXPECT_EQ(ui_info.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3));
  EXPECT_EQ(ui_info.is_google_pay_branding_enabled, true);
}

// Tests that CreateForUploadSave() sets confirm text to "accept" when nothing
// more is requested from the user.
TEST(AutofillSaveCardUiInfoTest,
     CreateForUploadSaveSetsConfirmTextWhenNoPrompt) {
  CreditCard card = test::GetMaskedServerCard();

  auto ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
      /*options=*/{}, card, LegalMessageLines(), AccountInfo());

  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
}

// Tests that CreateForLocalSave() sets confirm text to "continue" when the
// expiration is requested from the user.
TEST(AutofillSaveCardUiInfoTest,
     CreateForLocalSaveSetsConfirmTextWhenRequestingExpirationFromUser) {
  CreditCard card = test::GetCreditCard();

  auto ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
      /*options=*/
      {
          .should_request_expiration_date_from_user = true,
      },
      card, LegalMessageLines(), AccountInfo());

  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE));
}

// Tests that CreateForLocalSave() sets confirm text to "continue" when the name
// is requested from the user.
TEST(AutofillSaveCardUiInfoTest,
     CreateForUploadSaveSetsConfirmTextWhenRequestingNameFromUser) {
  CreditCard card = test::GetMaskedServerCard();

  auto ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
      /*options=*/
      {
          .should_request_name_from_user = true,
      },
      card, LegalMessageLines(), AccountInfo());

  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE));
}

}  // namespace autofill
