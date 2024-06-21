// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_iban_ui_info.h"

#include <string>

#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

constexpr char16_t kEllipsisOneDot[] = u"\u2022";
constexpr char16_t kEllipsisOneSpace[] = u"\u2006";

// A helper function to format the IBAN value returned by
// GetIdentifierStringForAutofillDisplay(), replacing the ellipsis ('\u2006')
// with a whitespace and oneDot ('\u2022') with '*'.
std::u16string FormatIbanForDisplay(std::u16string identifierIbanValue) {
  base::ReplaceChars(identifierIbanValue, kEllipsisOneSpace, u" ",
                     &identifierIbanValue);
  base::ReplaceChars(identifierIbanValue, kEllipsisOneDot, u"*",
                     &identifierIbanValue);
  return identifierIbanValue;
}

TEST(AutofillSaveIbanUiInfo, CreateForLocalSaveSetsProperties) {
  Iban localIban(
      Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  localIban.set_value(u"CH5604835012345678009");

  auto ui_info = AutofillSaveIbanUiInfo::CreateForLocalSave(
      localIban.GetIdentifierStringForAutofillDisplay());

  EXPECT_EQ(FormatIbanForDisplay(ui_info.iban_label),
            u"CH** **** **** **** *800 9");
  EXPECT_EQ(ui_info.title_text, l10n_util::GetStringUTF16(
                                    IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_LOCAL));
  EXPECT_EQ(ui_info.accept_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_ACCEPT));
  EXPECT_EQ(ui_info.cancel_text,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_NO_THANKS));
}

}  // namespace autofill
