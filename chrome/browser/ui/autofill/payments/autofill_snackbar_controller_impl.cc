// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller_impl.h"

#include <optional>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"
#include "chrome/browser/ui/android/autofill/snackbar/autofill_snackbar_view_android.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillSnackbarControllerImpl::AutofillSnackbarControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AutofillSnackbarControllerImpl::~AutofillSnackbarControllerImpl() {
  // If the tab is killed then dismiss the snackbar if it's showing.
  Dismiss();
}

void AutofillSnackbarControllerImpl::Show(
    AutofillSnackbarType autofill_snackbar_type) {
  ShowWithDurationAndCallback(autofill_snackbar_type, kDefaultSnackbarDuration,
                              std::nullopt);
}

void AutofillSnackbarControllerImpl::ShowWithDurationAndCallback(
    AutofillSnackbarType autofill_snackbar_type,
    base::TimeDelta snackbar_duration,
    std::optional<base::OnceClosure> on_dismiss_callback) {
  CHECK_NE(autofill_snackbar_type, AutofillSnackbarType::kUnspecified);
  if (autofill_snackbar_view_) {
    // A snackbar is already showing. Ignore the new request.
    return;
  }

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

void AutofillSnackbarControllerImpl::OnActionClicked() {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Snackbar.", GetSnackbarTypeForLogging(),
                    ".ActionClicked"}),
      true);

  switch (autofill_snackbar_type_) {
    case AutofillSnackbarType::kVirtualCard:
      ManualFillingControllerImpl::GetOrCreate(GetWebContents())
          ->ShowAccessorySheetTab(autofill::AccessoryTabType::CREDIT_CARDS);
      break;
    case AutofillSnackbarType::kMandatoryReauth:
      // For mandatory reauth snackbar, we will show Android credit card
      // settings page.
      ShowAutofillCreditCardSettings(GetWebContents());
      break;
    case AutofillSnackbarType::kSaveCardSuccess:
    case AutofillSnackbarType::kVirtualCardEnrollSuccess:
    case AutofillSnackbarType::kSaveServerIbanSuccess:
      // SnackbarManager.java will dismiss the snackbar after the click.
      break;
    case AutofillSnackbarType::kUnspecified:
      NOTREACHED();
  }
}

void AutofillSnackbarControllerImpl::OnDismissed() {
  autofill_snackbar_view_ = nullptr;
  autofill_snackbar_type_ = AutofillSnackbarType::kUnspecified;
  autofill_snackbar_duration_ = kDefaultSnackbarDuration;

  if (on_dismiss_callback_) {
    std::move(*on_dismiss_callback_).Run();
    on_dismiss_callback_.reset();
  }
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
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT);
    case AutofillSnackbarType::kVirtualCardEnrollSuccess:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT);
    case AutofillSnackbarType::kSaveServerIbanSuccess:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_SERVER_IBAN_SUCCESS_SNACKBAR_MESSAGE_TEXT);
    case AutofillSnackbarType::kUnspecified:
      NOTREACHED();
  }
}

std::u16string AutofillSnackbarControllerImpl::GetActionButtonText() const {
  switch (autofill_snackbar_type_) {
    case AutofillSnackbarType::kVirtualCard:
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

void AutofillSnackbarControllerImpl::Dismiss() {
  if (!autofill_snackbar_view_) {
    return;
  }

  autofill_snackbar_view_->Dismiss();
}

std::string AutofillSnackbarControllerImpl::GetSnackbarTypeForLogging() {
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
    case AutofillSnackbarType::kUnspecified:
      return "Unspecified";
  }
}

}  // namespace autofill
