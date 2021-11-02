// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller_impl.h"

#include <string>

#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_view.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"

namespace autofill {

CardUnmaskAuthenticationSelectionDialogControllerImpl::
    ~CardUnmaskAuthenticationSelectionDialogControllerImpl() {
  // This part of code is executed only if the browser window is closed when the
  // dialog is visible. In this case the controller is destroyed before
  // CardUnmaskAuthenticationSelectionDialogViews::dtor() is called,
  // but the reference to controller is not reset. This reference needs to be
  // reset via
  // CardUnmaskAuthenticationSelectionDialogView::Dismiss() to avoid a crash.
  if (dialog_view_)
    dialog_view_->Dismiss(/*user_closed_dialog=*/true);
}

// Static
CardUnmaskAuthenticationSelectionDialogControllerImpl*
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetOrCreate(
    content::WebContents* web_contents) {
  CardUnmaskAuthenticationSelectionDialogControllerImpl::CreateForWebContents(
      web_contents);
  CardUnmaskAuthenticationSelectionDialogControllerImpl* controller =
      CardUnmaskAuthenticationSelectionDialogControllerImpl::FromWebContents(
          web_contents);
  DCHECK(controller);
  return controller;
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::ShowDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmasking_method_callback,
    base::OnceClosure cancel_unmasking_closure) {
  if (dialog_view_)
    return;

  // Currently we only display the first challenge option available.
  DCHECK(!challenge_options.empty());
  challenge_options_ = {challenge_options[0]};

  confirm_unmasking_method_callback_ =
      std::move(confirm_unmasking_method_callback);
  cancel_unmasking_closure_ = std::move(cancel_unmasking_closure);

  dialog_view_ = CardUnmaskAuthenticationSelectionDialogView::CreateAndShow(
      this, web_contents());
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::
    DismissDialogUponServerAcceptAuthenticationMethod() {
  if (!dialog_view_)
    return;

  dialog_view_->Dismiss(/*user_closed_dialog=*/false);
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::OnDialogClosed(
    bool user_closed_dialog) {
  if (user_closed_dialog)
    std::move(cancel_unmasking_closure_).Run();
  else
    cancel_unmasking_closure_.Reset();

  dialog_view_ = nullptr;
  confirm_unmasking_method_callback_.Reset();
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::OnOkButtonClicked(
    const std::string& selected_challenge_option_id) {
  std::move(confirm_unmasking_method_callback_)
      .Run(selected_challenge_option_id);
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE_V2);
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetContentHeaderText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_ISSUER_CONFIRMATION_TEXT);
}

const std::vector<CardUnmaskChallengeOption>&
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetChallengeOptions()
    const {
  return challenge_options_;
}

ui::ImageModel CardUnmaskAuthenticationSelectionDialogControllerImpl::
    GetAuthenticationModeIcon(
        const CardUnmaskChallengeOption& challenge_option) const {
  switch (challenge_option.type) {
    case CardUnmaskChallengeOptionType::kSmsOtp:
      return ui::ImageModel::FromVectorIcon(vector_icons::kSmsIcon);
    case CardUnmaskChallengeOptionType::kUnknownType:
      NOTREACHED();
      return ui::ImageModel();
  }
}

std::u16string CardUnmaskAuthenticationSelectionDialogControllerImpl::
    GetAuthenticationModeLabel(
        const CardUnmaskChallengeOption& challenge_option) const {
  switch (challenge_option.type) {
    case CardUnmaskChallengeOptionType::kSmsOtp:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AUTHENTICATION_MODE_TEXT_MESSAGE_LABEL);
    case CardUnmaskChallengeOptionType::kUnknownType:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetContentFooterText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CURRENT_INFO_NOT_SEEN_TEXT);
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetOkButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL);
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetProgressLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_PROGRESS_BAR_MESSAGE);
}

#if defined(UNIT_TEST)
CardUnmaskAuthenticationSelectionDialogView*
CardUnmaskAuthenticationSelectionDialogControllerImpl::
    GetDialogViewForTesting() {
  return dialog_view_;
}
#endif

CardUnmaskAuthenticationSelectionDialogControllerImpl::
    CardUnmaskAuthenticationSelectionDialogControllerImpl(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    CardUnmaskAuthenticationSelectionDialogControllerImpl);

}  // namespace autofill
