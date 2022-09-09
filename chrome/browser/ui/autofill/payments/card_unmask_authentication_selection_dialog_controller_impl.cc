// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller_impl.h"

#include <string>

#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_view.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"

namespace autofill {

CardUnmaskAuthenticationSelectionDialogControllerImpl::
    ~CardUnmaskAuthenticationSelectionDialogControllerImpl() {
  // This part of code is executed only if the browser window is closed when the
  // dialog is visible. In this case the controller is destroyed before
  // CardUnmaskAuthenticationSelectionDialogViews::dtor() is called,
  // but the reference to controller is not reset. This reference needs to be
  // reset via CardUnmaskAuthenticationSelectionDialogView::Dismiss() to avoid a
  // crash.
  if (dialog_view_)
    dialog_view_->Dismiss(/*user_closed_dialog=*/true,
                          /*server_success=*/false);
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
      this, &GetWebContents());

  DCHECK(dialog_view_);
  AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogShown();
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::
    DismissDialogUponServerProcessedAuthenticationMethodRequest(
        bool server_success) {
  if (!dialog_view_)
    return;

  dialog_view_->Dismiss(/*user_closed_dialog=*/false, server_success);
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::OnDialogClosed(
    bool user_closed_dialog,
    bool server_success) {
  if (user_closed_dialog) {
    AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogResultMetric(
        challenge_option_selected_
            ? AutofillMetrics::
                  CardUnmaskAuthenticationSelectionDialogResultMetric::
                      kCanceledByUserAfterSelection
            : AutofillMetrics::
                  CardUnmaskAuthenticationSelectionDialogResultMetric::
                      kCanceledByUserBeforeSelection);
    // |cancel_unmasking_closure_| can be null in tests.
    if (cancel_unmasking_closure_)
      std::move(cancel_unmasking_closure_).Run();
  } else {
    AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogResultMetric(
        server_success
            ? AutofillMetrics::
                  CardUnmaskAuthenticationSelectionDialogResultMetric::
                      kDismissedByServerRequestSuccess
            : AutofillMetrics::
                  CardUnmaskAuthenticationSelectionDialogResultMetric::
                      kDismissedByServerRequestFailure);
  }

  challenge_option_selected_ = false;
  dialog_view_ = nullptr;
  confirm_unmasking_method_callback_.Reset();
  cancel_unmasking_closure_.Reset();
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::OnOkButtonClicked(
    const std::string& selected_challenge_option_id) {
  // |confirm_unmasking_method_callback_| can be null in tests.
  if (confirm_unmasking_method_callback_) {
    std::move(confirm_unmasking_method_callback_)
        .Run(selected_challenge_option_id);
  }
  challenge_option_selected_ = true;
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

CardUnmaskAuthenticationSelectionDialogControllerImpl::
    CardUnmaskAuthenticationSelectionDialogControllerImpl(
        content::WebContents* web_contents)
    : content::WebContentsUserData<
          CardUnmaskAuthenticationSelectionDialogControllerImpl>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    CardUnmaskAuthenticationSelectionDialogControllerImpl);

}  // namespace autofill
