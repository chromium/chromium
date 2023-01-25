// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_iban_bubble_controller_impl.h"

#include <string>

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// static
SaveIbanBubbleController* SaveIbanBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  SaveIbanBubbleControllerImpl::CreateForWebContents(web_contents);
  return SaveIbanBubbleControllerImpl::FromWebContents(web_contents);
}

SaveIbanBubbleControllerImpl::~SaveIbanBubbleControllerImpl() = default;

void SaveIbanBubbleControllerImpl::OfferLocalSave(
    const IBAN& iban,
    bool should_show_prompt,
    AutofillClient::LocalSaveIBANPromptCallback save_iban_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }

  iban_ = iban;
  local_save_iban_prompt_callback_ = std::move(save_iban_prompt_callback);
  current_bubble_type_ = IbanBubbleType::kLocalSave;

  if (should_show_prompt) {
    ShowBubble();
  }
  // TODO(crbug.com/1349109): Add else block for show Icon only.
}

std::u16string SaveIbanBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_LOCAL);
    case IbanBubbleType::kInactive:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string SaveIbanBubbleControllerImpl::GetAcceptButtonText() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_BUBBLE_LOCAL_SAVE_ACCEPT);
    case IbanBubbleType::kInactive:
      return std::u16string();
  }
}

std::u16string SaveIbanBubbleControllerImpl::GetDeclineButtonText() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_BUBBLE_LOCAL_SAVE_NO_THANKS);
    case IbanBubbleType::kInactive:
      return std::u16string();
  }
}

const IBAN& SaveIbanBubbleControllerImpl::GetIBAN() const {
  return iban_;
}

AutofillBubbleBase* SaveIbanBubbleControllerImpl::GetSaveBubbleView() const {
  return bubble_view();
}

void SaveIbanBubbleControllerImpl::OnSaveButton(
    const std::u16string& nickname) {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      DCHECK(!local_save_iban_prompt_callback_.is_null());
      std::move(local_save_iban_prompt_callback_)
          .Run(AutofillClient::SaveIBANOfferUserDecision::kAccepted, nickname);
      break;
    case IbanBubbleType::kInactive:
      NOTREACHED();
  }
}

void SaveIbanBubbleControllerImpl::OnCancelButton() {
  if (current_bubble_type_ == IbanBubbleType::kLocalSave) {
    std::move(local_save_iban_prompt_callback_)
        .Run(AutofillClient::SaveIBANOfferUserDecision::kDeclined,
             absl::nullopt);
  }
}

void SaveIbanBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);

  current_bubble_type_ = IbanBubbleType::kInactive;

  UpdatePageActionIcon();
}

SaveIbanBubbleControllerImpl::SaveIbanBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<SaveIbanBubbleControllerImpl>(*web_contents),
      personal_data_manager_(
          PersonalDataManagerFactory::GetInstance()->GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
}

PageActionIconType SaveIbanBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveIban;
}

void SaveIbanBubbleControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  set_bubble_view(
      browser->window()->GetAutofillBubbleHandler()->ShowSaveIbanBubble(
          web_contents(), this, /*is_user_gesture=*/false));
  DCHECK(bubble_view());

  if (observer_for_testing_) {
    observer_for_testing_->OnBubbleShown();
  }
}

void SaveIbanBubbleControllerImpl::ShowBubble() {
  DCHECK(current_bubble_type_ != IbanBubbleType::kInactive);
  // Local save callback should not be null for kLocalSave state.
  DCHECK(!(local_save_iban_prompt_callback_.is_null() &&
           current_bubble_type_ == IbanBubbleType::kLocalSave));
  DCHECK(!bubble_view());
  Show();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveIbanBubbleControllerImpl);

}  // namespace autofill
