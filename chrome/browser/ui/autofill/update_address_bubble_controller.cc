// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/update_address_bubble_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

UpdateAddressBubbleController::UpdateAddressBubbleController(
    base::WeakPtr<AddressBubbleControllerDelegate> delegate,
    content::WebContents* web_contents,
    const AutofillProfile& profile_to_save,
    const AutofillProfile& original_profile)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate),
      profile_to_save_(profile_to_save),
      original_profile_(original_profile) {}

UpdateAddressBubbleController::~UpdateAddressBubbleController() = default;

std::u16string UpdateAddressBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
}

std::u16string UpdateAddressBubbleController::GetFooterMessage() const {
  if (profile_to_save_.IsAccountProfile() && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_UPDATE_PROMPT_ACCOUNT_ADDRESS_SOURCE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  return {};
}

const AutofillProfile& UpdateAddressBubbleController::GetProfileToSave() const {
  return profile_to_save_;
}

const AutofillProfile& UpdateAddressBubbleController::GetOriginalProfile()
    const {
  return original_profile_;
}

void UpdateAddressBubbleController::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  if (delegate_) {
    delegate_->OnUserDecision(decision, profile);
  }
}

void UpdateAddressBubbleController::OnEditButtonClicked() {
  if (delegate_) {
    delegate_->ShowEditor(profile_to_save_, /*title_override=*/u"",
                          GetFooterMessage(),
                          /*is_editing_existing_address=*/true);
  }
}

void UpdateAddressBubbleController::OnBubbleClosed() {
  if (delegate_) {
    delegate_->OnBubbleClosed();
  }
}
}  // namespace autofill
