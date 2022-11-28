// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller_impl.h"

#include <string>

#include "base/check_is_test.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/common/autofill_payments_features.h"
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
  if (dialog_view_) {
    dialog_view_->Dismiss(/*user_closed_dialog=*/true,
                          /*server_success=*/false);
  }
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

  CHECK(!challenge_options.empty());
  challenge_options_ =
      base::FeatureList::IsEnabled(features::kAutofillEnableCvcForVcnYellowPath)
          ? challenge_options
          : std::vector<CardUnmaskChallengeOption>{challenge_options[0]};

  confirm_unmasking_method_callback_ =
      std::move(confirm_unmasking_method_callback);
  cancel_unmasking_closure_ = std::move(cancel_unmasking_closure);

  dialog_view_ = CardUnmaskAuthenticationSelectionDialog::CreateAndShow(
      this, &GetWebContents());

  DCHECK(dialog_view_);
  AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogShown(
      challenge_options_.size());
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
    // `user_closed_dialog` is only true when the user clicked cancel on the
    // dialog.
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
  } else if (selected_challenge_option_type_ ==
             CardUnmaskChallengeOptionType::kSmsOtp) {
    // If we have an SMS OTP challenge selected and `user_closed_dialog` is
    // false, that means that the user accepted the dialog after selecting the
    // SMS OTP challenge option, and we have a server response returned since we
    // immediately send a SelectChallengeOption request to the server and only
    // close the dialog once a response is returned. The SelectChallengeOption
    // request is sent to the payments server to generate an SMS OTP with the
    // bank or issuer and send it to the user.
    AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogResultMetric(
        server_success
            ? AutofillMetrics::
                  CardUnmaskAuthenticationSelectionDialogResultMetric::
                      kDismissedByServerRequestSuccess
            : AutofillMetrics::
                  CardUnmaskAuthenticationSelectionDialogResultMetric::
                      kDismissedByServerRequestFailure);
  } else if (selected_challenge_option_type_ ==
             CardUnmaskChallengeOptionType::kCvc) {
    // If we have a CVC challenge selected and `user_closed_dialog` is false,
    // that means that the user accepted the dialog after selecting the CVC
    // challenge option. `server_success` is not used in this case because we do
    // not send a SelectChallengeOption request in the case of a CVC challenge
    // selected, since we do not need to send the user any type of OTP. Thus, we
    // immediately render the CVC input dialog.
    AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogResultMetric(
        AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
            kDismissedByUserAcceptanceNoServerRequestNeeded);
  }

  challenge_option_selected_ = false;
  dialog_view_ = nullptr;
  confirm_unmasking_method_callback_.Reset();
  cancel_unmasking_closure_.Reset();
  selected_challenge_option_id_ = std::string();
  selected_challenge_option_type_ = CardUnmaskChallengeOptionType::kUnknownType;
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::
    OnOkButtonClicked() {
  DCHECK(!selected_challenge_option_id_.empty());

  auto selected_challenge_option = std::find_if(
      challenge_options_.begin(), challenge_options_.end(),
      [this](const CardUnmaskChallengeOption challenge_option) {
        return challenge_option.id == selected_challenge_option_id_;
      });

  DCHECK(selected_challenge_option != challenge_options_.end());
  selected_challenge_option_type_ = (*selected_challenge_option).type;

  DCHECK(selected_challenge_option_type_ !=
         CardUnmaskChallengeOptionType::kUnknownType);
  challenge_option_selected_ = true;

  if (!confirm_unmasking_method_callback_) {
    CHECK_IS_TEST();
  } else {
    std::move(confirm_unmasking_method_callback_)
        .Run(selected_challenge_option_id_);
  }

  if (dialog_view_) {
    switch (selected_challenge_option_type_) {
      case CardUnmaskChallengeOptionType::kCvc:
        // For CVC flow, skip the OTP pending dialog since we go straight to the
        // Card Unmask Prompt.
        dialog_view_->Dismiss(/*user_closed_dialog=*/false,
                              /*server_success=*/false);
        break;
      case CardUnmaskChallengeOptionType::kSmsOtp:
        // Show the OTP pending dialog.
        dialog_view_->UpdateContent();
        break;
      case CardUnmaskChallengeOptionType::kUnknownType:
        NOTREACHED();
        break;
    }
  }
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      GetChallengeOptions().size() > 1
          ? IDS_AUTOFILL_CARD_AUTH_SELECTION_DIALOG_TITLE_MULTIPLE_OPTIONS
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE_V2);
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
    case CardUnmaskChallengeOptionType::kCvc:
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
          GetChallengeOptions().size() > 1
              ? IDS_AUTOFILL_AUTHENTICATION_MODE_GET_TEXT_MESSAGE
              : IDS_AUTOFILL_AUTHENTICATION_MODE_TEXT_MESSAGE);
    case CardUnmaskChallengeOptionType::kCvc:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AUTHENTICATION_MODE_SECURITY_CODE);
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
      GetChallengeOptions().size() > 1
          ? IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_CONTINUE
          : IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_SEND);
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetProgressLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_PROGRESS_BAR_MESSAGE);
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::
    SetSelectedChallengeOptionId(
        const std::string& selected_challenge_option_id) {
  selected_challenge_option_id_ = selected_challenge_option_id;
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
