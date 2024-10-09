// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_iban_ui_info.h"

#include <string>

#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

constexpr char16_t kEllipsisOneSpace[] = u"\u2006";

// A helper function to format the IBAN value returned by
// GetIdentifierStringForAutofillDisplay(), replacing the ellipsis ('\u2006')
// with a whitespace.
std::u16string FormatIbanForDisplay(std::u16string identifierIbanValue) {
  base::ReplaceChars(identifierIbanValue, kEllipsisOneSpace, u" ",
                     &identifierIbanValue);
  return identifierIbanValue;
}

TEST(AutofillSaveIbanUiInfo, CreateForLocalSaveSetsProperties) {
  Iban localIban(
      Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  localIban.set_value(u"CH5604835012345678009");

  auto ui_info = AutofillSaveIbanUiInfo::CreateForLocalSave(
      localIban.GetIdentifierStringForAutofillDisplay(
          /*is_value_masked=*/false));

  EXPECT_FALSE(ui_info.is_server_save);
  EXPECT_EQ(ui_info.logo_icon_id, 0);
  EXPECT_EQ(FormatIbanForDisplay(ui_info.iban_value),
            u"CH56 0483 5012 3456 7800 9");
  EXPECT_EQ(ui_info.title_text, l10n_util::GetStringUTF16(
                                    IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_LOCAL));
  EXPECT_EQ(ui_info.description_text, std::u16string());
  EXPECT_EQ(ui_info.accept_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_ACCEPT));
  EXPECT_EQ(ui_info.cancel_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_NO_THANKS));
  EXPECT_THAT(ui_info.legal_message_lines, testing::ElementsAre());
}

TEST(AutofillSaveIbanUiInfo, CreateForUploadSaveSetsProperties) {
  Iban localIban(
      Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  localIban.set_value(u"FR7630006000011234567890189");

  auto ui_info = AutofillSaveIbanUiInfo::CreateForUploadSave(
      localIban.GetIdentifierStringForAutofillDisplay(
          /*is_value_masked=*/false),
      LegalMessageLines());

  EXPECT_TRUE(ui_info.is_server_save);
  EXPECT_EQ(ui_info.logo_icon_id, IDR_AUTOFILL_GOOGLE_PAY);
  EXPECT_EQ(FormatIbanForDisplay(ui_info.iban_value),
            u"FR76 3000 6000 0112 3456 7890 189");
  EXPECT_EQ(
      ui_info.title_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_SERVER));
  EXPECT_EQ(
      ui_info.description_text,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_UPLOAD_IBAN_PROMPT_EXPLANATION));
  EXPECT_EQ(ui_info.accept_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_ACCEPT));
  EXPECT_EQ(ui_info.cancel_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_NO_THANKS));
  EXPECT_THAT(ui_info.legal_message_lines, testing::ElementsAre());
}

}  // namespace autofill
