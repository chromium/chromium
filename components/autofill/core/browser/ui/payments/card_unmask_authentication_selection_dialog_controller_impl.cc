// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"

#include <string>

#include "base/check_is_test.h"
#include "base/not_fatal_until.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

namespace autofill {

CardUnmaskAuthenticationSelectionDialogControllerImpl::
    CardUnmaskAuthenticationSelectionDialogControllerImpl(
        const std::vector<CardUnmaskChallengeOption>& challenge_options,
        base::OnceCallback<void(const std::string&)>
            confirm_unmasking_method_callback,
        base::OnceClosure cancel_unmasking_closure)
    : challenge_options_(challenge_options),
      confirm_unmasking_method_callback_(
          std::move(confirm_unmasking_method_callback)),
      cancel_unmasking_closure_(std::move(cancel_unmasking_closure)) {
  CHECK(!challenge_options_.empty());
#if BUILDFLAG(IS_IOS)
  selected_challenge_option_id_ = challenge_options_[0].id;
#endif  // BUILDFLAG(IS_IOS)
}

CardUnmaskAuthenticationSelectionDialogControllerImpl::
    ~CardUnmaskAuthenticationSelectionDialogControllerImpl() {
  // This part of code is executed only if the browser window is closed when the
  // dialog is visible, or if the user re-triggers the challenge selection flow
  // after not completing it previously. In this case the controller is
  // destroyed before CardUnmaskAuthenticationSelectionDialogViews::dtor() is
  // called, but the reference to controller is not reset. This reference needs
  // to be reset via CardUnmaskAuthenticationSelectionDialogView::Dismiss() to
  // avoid a crash.
  if (dialog_view_) {
    dialog_view_->Dismiss(/*user_closed_dialog=*/true,
                          /*server_success=*/false);
  }
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::ShowDialog(
    CardUnmaskAuthenticationSelectionDialogControllerImpl::CreateAndShowCallback
        create_and_show_callback) {
  dialog_view_ = std::move(create_and_show_callback).Run(this);

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
                 CardUnmaskChallengeOptionType::kSmsOtp ||
             selected_challenge_option_type_ ==
                 CardUnmaskChallengeOptionType::kEmailOtp) {
    // If we have an OTP challenge selected and `user_closed_dialog` is false,
    // that means that the user accepted the dialog after selecting the OTP
    // challenge option, and we have a server response returned since we
    // immediately send a SelectChallengeOption request to the server and only
    // close the dialog once a response is returned. The SelectChallengeOption
    // request is sent to the payments server to generate an OTP with the bank
    // or issuer and send it to the user.
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
  selected_challenge_option_id_ =
      CardUnmaskChallengeOption::ChallengeOptionId();
  selected_challenge_option_type_ = CardUnmaskChallengeOptionType::kUnknownType;
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::
    OnOkButtonClicked() {
  DCHECK(!selected_challenge_option_id_.value().empty());

  // TODO(crbug.com/40247983): Remove this lambda once we refactor
  // `SetSelectedChallengeOptionId()` to `SetSelectedChallengeOptionForId()`.
  auto selected_challenge_option =
      base::ranges::find(challenge_options_, selected_challenge_option_id_,
                         &CardUnmaskChallengeOption::id);

  CHECK(selected_challenge_option != challenge_options_.end(),
        base::NotFatalUntil::M130);
  selected_challenge_option_type_ = (*selected_challenge_option).type;

  DCHECK(selected_challenge_option_type_ !=
         CardUnmaskChallengeOptionType::kUnknownType);
  challenge_option_selected_ = true;

  if (!confirm_unmasking_method_callback_) {
    CHECK_IS_TEST();
  } else {
    std::move(confirm_unmasking_method_callback_)
        .Run(selected_challenge_option_id_.value());
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
      case CardUnmaskChallengeOptionType::kEmailOtp:
        // Show the OTP pending dialog.
        dialog_view_->UpdateContent();
        break;
      case CardUnmaskChallengeOptionType::kThreeDomainSecure:
        // TODO(crbug.com/41494927): Add kThreeDomainSecure logic.
      case CardUnmaskChallengeOptionType::kUnknownType:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      GetChallengeOptions().size() > 1
          ? IDS_AUTOFILL_CARD_AUTH_SELECTION_DIALOG_TITLE_MULTIPLE_OPTIONS
          : IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_TITLE);
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
    case CardUnmaskChallengeOptionType::kEmailOtp:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AUTHENTICATION_MODE_GET_EMAIL);
    case CardUnmaskChallengeOptionType::kThreeDomainSecure:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AUTHENTICATION_MODE_THREE_DOMAIN_SECURE);
    case CardUnmaskChallengeOptionType::kUnknownType:
      NOTREACHED_IN_MIGRATION();
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
  // TODO(crbug.com/40247983): Remove this lambda once we refactor
  // `SetSelectedChallengeOptionId()` to `SetSelectedChallengeOptionForId()`.
  auto selected_challenge_option =
      base::ranges::find(challenge_options_, selected_challenge_option_id_,
                         &CardUnmaskChallengeOption::id);
  switch (selected_challenge_option->type) {
    case CardUnmaskChallengeOptionType::kSmsOtp:
    case CardUnmaskChallengeOptionType::kEmailOtp:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_SEND);
    case CardUnmaskChallengeOptionType::kCvc:
    case CardUnmaskChallengeOptionType::kThreeDomainSecure:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_OK_BUTTON_LABEL_CONTINUE);
    case CardUnmaskChallengeOptionType::kUnknownType:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

std::u16string
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetProgressLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_PROGRESS_BAR_MESSAGE);
}

void CardUnmaskAuthenticationSelectionDialogControllerImpl::
    SetSelectedChallengeOptionId(
        const CardUnmaskChallengeOption::ChallengeOptionId&
            selected_challenge_option_id) {
  selected_challenge_option_id_ = selected_challenge_option_id;
}

base::WeakPtr<CardUnmaskAuthenticationSelectionDialogControllerImpl>
CardUnmaskAuthenticationSelectionDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
