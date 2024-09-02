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
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/grit/components_scaled_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {
namespace {

using CardSaveType = payments::PaymentsAutofillClient::CardSaveType;

MATCHER_P(HasLegalMessageLineText, text, "A LegalMessageLine that has text.") {
  return base::UTF16ToUTF8(arg.text()) == text;
}

// The different representations of a year needed by tests here.
struct Year {
  int integer;
  std::string string;
  std::u16string u16string;
  std::string last2_string;
};

Year SetUpNextYear() {
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

AutofillSaveCardUiInfo AutofillSaveCardUiInfoForUploadSaveForTest(
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    bool is_gpay_branded = false) {
  return AutofillSaveCardUiInfo::CreateForUploadSave(
      options, test::GetMaskedServerCard(), LegalMessageLines(), AccountInfo(),
      is_gpay_branded);
}

// Verify that the AutofillSaveCardUiInfo attributes for local save that are
// common across all prompts for are correctly set.
TEST(AutofillSaveCardUiInfoTestForLocalSave, VerifyCommonAttributes) {
  const Year next_year = SetUpNextYear();
  CreditCard card = test::GetCreditCard();
  test::SetCreditCardInfo(&card, "Chromium Dev", "4111111111111111", "3",
                          next_year.string.c_str(), "");
  card.SetNickname(u"My Card");
  auto ui_info =
      AutofillSaveCardUiInfo::CreateForLocalSave(/*options=*/{}, card);
  EXPECT_EQ(ui_info.issuer_icon_id,
            base::FeatureList::IsEnabled(
                features::kAutofillEnableNewCardArtAndNetworkImages)
                ? IDR_AUTOFILL_METADATA_CC_VISA
                : IDR_AUTOFILL_CC_VISA);
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
  EXPECT_EQ(ui_info.is_for_upload, false);
  EXPECT_THAT(ui_info.legal_message_lines, testing::ElementsAre());
  EXPECT_EQ(ui_info.displayed_target_account_email, u"");
  EXPECT_TRUE(ui_info.displayed_target_account_avatar.IsEmpty());
  EXPECT_EQ(ui_info.cancel_text, l10n_util::GetStringUTF16(
                                     IDS_AUTOFILL_NO_THANKS_MOBILE_LOCAL_SAVE));
  EXPECT_EQ(ui_info.is_google_pay_branding_enabled, false);
}

// Verify that AutofillSaveCardUiInfo attributes are correctly set for the
// local-card-only-save infobar.
TEST(AutofillSaveCardUiInfoTestForLocalSave,
     VerifyAttributesForCardSaveOnlyInfobar) {
  auto ui_info = AutofillSaveCardUiInfo::CreateForLocalSave(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveOnly},
      test::GetCreditCard());

  EXPECT_EQ(ui_info.logo_icon_id, IDR_INFOBAR_AUTOFILL_CC);
  EXPECT_EQ(ui_info.title_text, l10n_util::GetStringUTF16(
                                    IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL));
  EXPECT_EQ(ui_info.description_text, u"");
  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
}

#if BUILDFLAG(IS_ANDROID)
// Verify that AutofillSaveCardUiInfo attributes are correctly set for the
// local-card-only-save bottom sheet.
TEST(AutofillSaveCardUiInfoTestForLocalSave,
     VerifyAttributesForCardSaveOnlyBottomSheet) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  auto ui_info = AutofillSaveCardUiInfo::CreateForLocalSave(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveOnly},
      test::GetCreditCard());

  EXPECT_EQ(ui_info.logo_icon_id, IDR_INFOBAR_AUTOFILL_CC);
  EXPECT_EQ(ui_info.title_text, l10n_util::GetStringUTF16(
                                    IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL));
  EXPECT_EQ(ui_info.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_ONLY_PROMPT_EXPLANATION_LOCAL));
  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
}

// Verify that AutofillSaveCardUiInfo attributes are correctly set for the
// local-card-save-with-CVC bottom sheet.
TEST(AutofillSaveCardUiInfoTestForLocalSave,
     VerifyAttributesForCardSaveWithCvcBottomSheet) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  auto ui_info = AutofillSaveCardUiInfo::CreateForLocalSave(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveWithCvc},
      test::GetCreditCard());

  EXPECT_EQ(ui_info.logo_icon_id, IDR_INFOBAR_AUTOFILL_CC);
  EXPECT_EQ(ui_info.title_text, l10n_util::GetStringUTF16(
                                    IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL));
  EXPECT_EQ(ui_info.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_WITH_CVC_PROMPT_EXPLANATION_LOCAL));
  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
}

// Verify that AutofillSaveCardUiInfo attributes are correctly set for the
// local-CVC-only-save Message.
TEST(AutofillSaveCardUiInfoTestForLocalSave,
     VerifyAttributesForCvcSaveOnlyMessage) {
  auto ui_info = AutofillSaveCardUiInfo::CreateForLocalSave(
      /*options=*/{.card_save_type = CardSaveType::kCvcSaveOnly},
      test::GetCreditCard());

  EXPECT_EQ(ui_info.logo_icon_id, IDR_AUTOFILL_CC_GENERIC_PRIMARY);
  EXPECT_EQ(ui_info.title_text, l10n_util::GetStringUTF16(
                                    IDS_AUTOFILL_SAVE_CVC_PROMPT_TITLE_LOCAL));
  EXPECT_EQ(ui_info.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CVC_PROMPT_EXPLANATION_LOCAL));
  EXPECT_EQ(
      ui_info.confirm_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CVC_MESSAGE_SAVE_ACCEPT));
}
#endif  // #BUILDFLAG(IS_ANDROID)

// Test class for testing the upload save card prompts. Parametrized tests are
// run with GPay branding enabled/disabled.
class AutofillSaveCardUiInfoTestForUploadSave
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  AutofillSaveCardUiInfoTestForUploadSave() = default;

  ~AutofillSaveCardUiInfoTestForUploadSave() override = default;

  bool is_gpay_branded() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillSaveCardUiInfoTestForUploadSave,
                         testing::Bool());

// Verify that the AutofillSaveCardUiInfo attributes for upload save that are
// common across all prompts for are correctly set.
TEST_P(AutofillSaveCardUiInfoTestForUploadSave, VerifyCommonAttributes) {
  const Year next_year = SetUpNextYear();
  CreditCard card = test::GetMaskedServerCard();
  card.SetNetworkForMaskedCard(kVisaCard);
  test::SetCreditCardInfo(&card, "Chromium Dev", "4111111111111111", "3",
                          next_year.string.c_str(), "");
  card.SetNickname(u"My Card");
  LegalMessageLines legal_message_lines(
      {TestLegalMessageLine("example message")});
  AccountInfo account_info;
  account_info.account_image = gfx::test::CreateImage(11, 17);
  account_info.email = "example email";

  auto ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
      /*options=*/{}, card, legal_message_lines, account_info,
      /*is_google_pay_branding_enabled=*/is_gpay_branded());
  EXPECT_EQ(ui_info.issuer_icon_id,
            base::FeatureList::IsEnabled(
                features::kAutofillEnableNewCardArtAndNetworkImages)
                ? IDR_AUTOFILL_METADATA_CC_VISA
                : IDR_AUTOFILL_CC_VISA);
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
  EXPECT_EQ(ui_info.is_for_upload, true);
  EXPECT_THAT(ui_info.legal_message_lines,
              testing::ElementsAre(HasLegalMessageLineText("example message")));
  EXPECT_EQ(ui_info.displayed_target_account_email, u"example email");
  EXPECT_EQ(ui_info.displayed_target_account_avatar.Size(), gfx::Size(11, 17));
  EXPECT_EQ(
      ui_info.cancel_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_NO_THANKS_MOBILE_UPLOAD_SAVE));
  EXPECT_EQ(ui_info.is_google_pay_branding_enabled, is_gpay_branded());
}

// Verify that AutofillSaveCardUiInfo attributes are correctly set for the
// upload-card-only-save infobar.
TEST_P(AutofillSaveCardUiInfoTestForUploadSave,
       VerifyAttributesForCardSaveOnlyInfobar) {
  auto ui_info = AutofillSaveCardUiInfoForUploadSaveForTest(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveOnly},
      is_gpay_branded());

  EXPECT_EQ(ui_info.logo_icon_id, is_gpay_branded() ? IDR_AUTOFILL_GOOGLE_PAY
                                                    : IDR_INFOBAR_AUTOFILL_CC);
  EXPECT_EQ(
      ui_info.title_text,
      l10n_util::GetStringUTF16(
          is_gpay_branded() ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3
                            : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD));
  EXPECT_EQ(ui_info.description_text,
            is_gpay_branded()
                ? l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3)
                : u"");
  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
}

#if BUILDFLAG(IS_ANDROID)
// Verify that AutofillSaveCardUiInfo attributes are correctly set for the
// upload-card-only-save bottom sheet.
TEST_P(AutofillSaveCardUiInfoTestForUploadSave,
       VerifyAttributesForCardSaveOnlyBottomSheet) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  auto ui_info = AutofillSaveCardUiInfoForUploadSaveForTest(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveOnly},
      is_gpay_branded());

  EXPECT_EQ(ui_info.logo_icon_id, is_gpay_branded() ? IDR_AUTOFILL_GOOGLE_PAY
                                                    : IDR_INFOBAR_AUTOFILL_CC);
  EXPECT_EQ(
      ui_info.title_text,
      l10n_util::GetStringUTF16(
          is_gpay_branded() ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3
                            : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD));
  EXPECT_EQ(ui_info.description_text,
            is_gpay_branded()
                ? l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3)
                : u"");
  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
}

// Verify that AutofillSaveCardUiInfo attributes are correctly set for the
// upload-card-save-with-CVC bottom sheet.
TEST_P(AutofillSaveCardUiInfoTestForUploadSave,
       VerifyAttributesForCardSaveWithCvcBottomSheet) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  auto ui_info = AutofillSaveCardUiInfoForUploadSaveForTest(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveWithCvc},
      is_gpay_branded());

  EXPECT_EQ(ui_info.logo_icon_id, is_gpay_branded() ? IDR_AUTOFILL_GOOGLE_PAY
                                                    : IDR_INFOBAR_AUTOFILL_CC);
  EXPECT_EQ(
      ui_info.title_text,
      l10n_util::GetStringUTF16(
          is_gpay_branded() ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3
                            : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD));
  EXPECT_EQ(ui_info.description_text,
            is_gpay_branded()
                ? l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_SAVE_CARD_WITH_CVC_PROMPT_EXPLANATION_UPLOAD)
                : u"");
  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
}

// Verify that AutofillSaveCardUiInfo attributes are correctly set for the
// upload-CVC-only-save message.
TEST_P(AutofillSaveCardUiInfoTestForUploadSave,
       VerifyAttributesForCvcSaveOnlyMessage) {
  auto ui_info = AutofillSaveCardUiInfoForUploadSaveForTest(
      /*options=*/{.card_save_type = CardSaveType::kCvcSaveOnly},
      is_gpay_branded());

  EXPECT_EQ(ui_info.logo_icon_id, IDR_AUTOFILL_CC_GENERIC_PRIMARY);
  EXPECT_EQ(
      ui_info.title_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CVC_PROMPT_TITLE_TO_CLOUD));
  EXPECT_EQ(ui_info.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CVC_PROMPT_EXPLANATION_UPLOAD));
  EXPECT_EQ(
      ui_info.confirm_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CVC_MESSAGE_SAVE_ACCEPT));
}
#endif  // BUILDFLAG(IS_ANDROID)

// Tests that the card description is set correctly when there is no nickname.
TEST(AutofillSaveCardUiInfoTestForUploadSave,
     CreateForUploadSaveSetsCardDescriptionWithoutNickname) {
  const Year next_year = SetUpNextYear();
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

// Tests that CreateForUploadSave() sets confirm text to "accept" when nothing
// more is requested from the user.
TEST(AutofillSaveCardUiInfoTestForUploadSave,
     CreateForUploadSaveSetsConfirmTextWhenNoPrompt) {
  auto ui_info = AutofillSaveCardUiInfoForUploadSaveForTest(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveOnly});

  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
}

// Tests that CreateForUploadSave() sets confirm text to "continue" when the
// expiration is requested from the user.
TEST(AutofillSaveCardUiInfoTestForUploadSave,
     CreateForUploadSaveSetsConfirmTextWhenRequestingExpirationFromUser) {
  auto ui_info = AutofillSaveCardUiInfoForUploadSaveForTest(
      /*options=*/
      {
          .should_request_expiration_date_from_user = true,
      });

  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE));
}

// Tests that CreateForUploadSave() sets confirm text to "continue" when the
// name is requested from the user.
TEST(AutofillSaveCardUiInfoTestForUploadSave,
     CreateForUploadSaveSetsConfirmTextWhenRequestingNameFromUser) {
  auto ui_info = AutofillSaveCardUiInfoForUploadSaveForTest(
      /*options=*/
      {
          .should_request_name_from_user = true,
      });

  EXPECT_EQ(ui_info.confirm_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE));
}

}  // namespace
}  // namespace autofill
