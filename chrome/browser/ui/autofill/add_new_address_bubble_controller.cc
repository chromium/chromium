// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/add_new_address_bubble_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AddNewAddressBubbleController::AddNewAddressBubbleController(
    content::WebContents* web_contents,
    bool save_into_account,
    base::WeakPtr<AddressBubbleControllerDelegate> delegate)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate),
      save_into_account_(save_into_account) {}

AddNewAddressBubbleController::~AddNewAddressBubbleController() = default;

std::u16string AddNewAddressBubbleController::GetBodyText() const {
  return l10n_util::GetStringUTF16(
      save_into_account_
          ? IDS_AUTOFILL_ADD_NEW_ADDRESS_INTO_ACCOUNT_PROMPT_BODY_TEXT
          : IDS_AUTOFILL_ADD_NEW_ADDRESS_INTO_CHROME_PROMPT_BODY_TEXT);
}

std::u16string AddNewAddressBubbleController::GetFooterMessage() const {
  if (save_into_account_ && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  return {};
}

void AddNewAddressBubbleController::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision) {
  if (delegate_) {
    delegate_->OnUserDecision(decision, std::nullopt);
  }
}

void AddNewAddressBubbleController::OnAddButtonClicked() {
  if (delegate_) {
    delegate_->ShowEditor(GetFooterMessage(),
                          /*is_editing_existing_address=*/false);
  }
}

void AddNewAddressBubbleController::OnBubbleClosed() {
  if (delegate_) {
    delegate_->OnBubbleClosed();
  }
}

}  // namespace autofill
