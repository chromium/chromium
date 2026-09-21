// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_snackbar_controller_impl.h"

#include <optional>
#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/ui/autofill/autofill_snackbar_type.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillSnackbarControllerImpl::AutofillSnackbarControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AutofillSnackbarControllerImpl::~AutofillSnackbarControllerImpl() {
  on_dismiss_callback_.reset();
  on_action_clicked_callback_.Reset();
  Dismiss();
}

void AutofillSnackbarControllerImpl::Show(
    AutofillSnackbarType autofill_snackbar_type,
    base::OnceClosure on_action_clicked_callback) {
  ShowWithDurationAndCallback(autofill_snackbar_type, kDefaultSnackbarDuration,
                              std::move(on_action_clicked_callback),
                              std::nullopt);
}

void AutofillSnackbarControllerImpl::ShowWithDurationAndCallback(
    AutofillSnackbarType autofill_snackbar_type,
    base::TimeDelta snackbar_duration,
    base::OnceClosure on_action_clicked_callback,
    std::optional<base::OnceClosure> on_dismiss_callback) {
  CHECK_NE(autofill_snackbar_type, AutofillSnackbarType::kUnspecified);
  while (autofill_snackbar_view_) {
    Dismiss();
  }
  CHECK(!autofill_snackbar_view_);

  on_action_clicked_callback_ = std::move(on_action_clicked_callback);
  on_dismiss_callback_ = std::move(on_dismiss_callback);

  autofill_snackbar_type_ = autofill_snackbar_type;
  autofill_snackbar_view_ = AutofillSnackbarView::Create(this);
  autofill_snackbar_duration_ = snackbar_duration;
  autofill_snackbar_view_->Show();
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Autofill.Snackbar.", GetSnackbarTypeForLogging(), ".Shown"}),
      true);
}

void AutofillSnackbarControllerImpl::ShowPaymentsSnackbar(
    AutofillSnackbarType type,
    const CreditCard& filled_card,
    base::OnceClosure on_action_clicked_callback) {
  while (autofill_snackbar_view_) {
    Dismiss();
  }
  filled_card_ = filled_card;
  Show(type, std::move(on_action_clicked_callback));
}

void AutofillSnackbarControllerImpl::OnActionClicked() {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Snackbar.", GetSnackbarTypeForLogging(),
                    ".ActionClicked"}),
      true);

  auto action_callback = std::move(on_action_clicked_callback_);
  auto dismiss_callback = std::exchange(on_dismiss_callback_, std::nullopt);
  ResetState();

  if (action_callback) {
    std::move(action_callback).Run();
  }
  if (dismiss_callback) {
    std::move(*dismiss_callback).Run();
  }
}

void AutofillSnackbarControllerImpl::OnDismissed() {
  ResetState();
  RunDismissCallbackIfAny();
}

std::u16string AutofillSnackbarControllerImpl::GetMessageText() const {
  switch (autofill_snackbar_type_) {
    case AutofillSnackbarType::kVirtualCard:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_NUMBER_SNACKBAR_MESSAGE_TEXT);
    case AutofillSnackbarType::kMandatoryReauth:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MANDATORY_REAUTH_SNACKBAR_MESSAGE_TEXT);
    case AutofillSnackbarType::kSaveCardSuccess:
      return l10n_util::GetStringUTF16(
          base::FeatureList::IsEnabled(
              features::kAutofillEnableWalletBrandingV2)
              ? IDS_AUTOFILL_SAVE_CARD_TO_WALLET_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT_V2
              : IDS_AUTOFILL_SAVE_CARD_TO_WALLET_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT);
    case AutofillSnackbarType::kVirtualCardEnrollSuccess:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT);
    case AutofillSnackbarType::kSaveServerIbanSuccess:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_SERVER_IBAN_TO_WALLET_SUCCESS_SNACKBAR_MESSAGE_TEXT);
    case AutofillSnackbarType::kCardInfoRetrieval:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_INFO_RETRIEVAL_SNACKBAR_MESSAGE_TEXT);
    case AutofillSnackbarType::kBnpl:
      CHECK(filled_card_);
      return l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_BNPL_FILLED_CARD_SNACKBAR_MESSAGE_TEXT,
          filled_card_->CardNameForAutofillDisplay());
    case AutofillSnackbarType::kAutofillAiSaveToWalletFailure:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_WALLET_UPDATE_OR_MIGRATE_FAILURE_NOTIFICATION);
    case AutofillSnackbarType::kAutofillAiFetchEntityFailure:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_FETCH_ENTITY_FAILURE_NOTIFICATION);
    case AutofillSnackbarType::kAutofillAiSuppressionUndo:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_SUPPRESSION_UNDO_SNACKBAR_MESSAGE);
    case AutofillSnackbarType::kUnspecified:
      NOTREACHED();
  }
}

std::u16string AutofillSnackbarControllerImpl::GetActionButtonText() const {
  switch (autofill_snackbar_type_) {
    case AutofillSnackbarType::kVirtualCard:
    case AutofillSnackbarType::kCardInfoRetrieval:
    case AutofillSnackbarType::kBnpl:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_NUMBER_SNACKBAR_ACTION_TEXT);
    case AutofillSnackbarType::kMandatoryReauth:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MANDATORY_REAUTH_SNACKBAR_ACTION_TEXT);
    case AutofillSnackbarType::kSaveCardSuccess:
    case AutofillSnackbarType::kVirtualCardEnrollSuccess:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUTTON_TEXT);
    case AutofillSnackbarType::kSaveServerIbanSuccess:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_SERVER_IBAN_SUCCESS_SNACKBAR_BUTTON_TEXT);
    case AutofillSnackbarType::kAutofillAiSaveToWalletFailure:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_SNACK_BAR_CONFIRMATION_BUTTON_LABEL);
    case AutofillSnackbarType::kAutofillAiFetchEntityFailure:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_SNACK_BAR_CONFIRMATION_BUTTON_LABEL);
    case AutofillSnackbarType::kAutofillAiSuppressionUndo:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_SUPPRESSION_UNDO_SNACKBAR_ACTION);
    case AutofillSnackbarType::kUnspecified:
      NOTREACHED();
  }
}

base::TimeDelta AutofillSnackbarControllerImpl::GetDuration() const {
  return autofill_snackbar_duration_;
}

content::WebContents* AutofillSnackbarControllerImpl::GetWebContents() const {
  return web_contents_;
}

AutofillSnackbarType AutofillSnackbarControllerImpl::GetSnackbarType() const {
  return autofill_snackbar_type_;
}

void AutofillSnackbarControllerImpl::Dismiss() {
  if (!autofill_snackbar_view_) {
    return;
  }
  ResetState();
  RunDismissCallbackIfAny();
}

void AutofillSnackbarControllerImpl::ResetState() {
  if (AutofillSnackbarView* const view =
          std::exchange(autofill_snackbar_view_, nullptr)) {
    view->Dismiss();
  }
  autofill_snackbar_type_ = AutofillSnackbarType::kUnspecified;
  autofill_snackbar_duration_ = kDefaultSnackbarDuration;
  on_action_clicked_callback_.Reset();
  filled_card_.reset();
}

void AutofillSnackbarControllerImpl::RunDismissCallbackIfAny() {
  if (auto callback = std::exchange(on_dismiss_callback_, std::nullopt)) {
    std::move(*callback).Run();
  }
}

std::string AutofillSnackbarControllerImpl::GetSnackbarTypeForLogging() const {
  switch (autofill_snackbar_type_) {
    case AutofillSnackbarType::kVirtualCard:
      return "VirtualCard";
    case AutofillSnackbarType::kMandatoryReauth:
      return "MandatoryReauth";
    case AutofillSnackbarType::kSaveCardSuccess:
      return "SaveCardSuccess";
    case AutofillSnackbarType::kVirtualCardEnrollSuccess:
      return "VirtualCardEnrollSuccess";
    case AutofillSnackbarType::kSaveServerIbanSuccess:
      return "SaveServerIbanSuccess";
    case AutofillSnackbarType::kCardInfoRetrieval:
      return "CardInfoRetrievalEnrolled";
    case AutofillSnackbarType::kBnpl:
      return "BnplVirtualCard";
    case AutofillSnackbarType::kAutofillAiSaveToWalletFailure:
      return "AutofillAiSaveToWalletFailure";
    case AutofillSnackbarType::kAutofillAiFetchEntityFailure:
      return "AutofillAiFetchFromWalletFailure";
    case AutofillSnackbarType::kAutofillAiSuppressionUndo:
      return "AutofillAiSuppressionUndo";
    case AutofillSnackbarType::kUnspecified:
      return "Unspecified";
  }
}

}  // namespace autofill
