// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller_impl.h"

#include <string>
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/android/preferences/autofill/settings_launcher_helper.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_controller_impl.h"
#include "chrome/browser/ui/android/autofill/snackbar/autofill_snackbar_view_android.h"
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
  CHECK_NE(autofill_snackbar_type, AutofillSnackbarType::kUnspecified);
  if (autofill_snackbar_view_) {
    // A snackbar is already showing. Ignore the new request.
    return;
  }
  autofill_snackbar_type_ = autofill_snackbar_type;
  autofill_snackbar_view_ = AutofillSnackbarView::Create(this);
  autofill_snackbar_view_->Show();
  base::UmaHistogramBoolean(
      "Autofill.Snackbar." + GetSnackbarTypeForLogging() + ".Shown", true);
}

void AutofillSnackbarControllerImpl::OnActionClicked() {
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
    case AutofillSnackbarType::kUnspecified:
      NOTREACHED_NORETURN();
  }
  base::UmaHistogramBoolean(
      "Autofill.Snackbar." + GetSnackbarTypeForLogging() + ".ActionClicked",
      true);
}

void AutofillSnackbarControllerImpl::OnDismissed() {
  autofill_snackbar_view_ = nullptr;
  autofill_snackbar_type_ = AutofillSnackbarType::kUnspecified;
}

std::u16string AutofillSnackbarControllerImpl::GetMessageText() const {
  switch (autofill_snackbar_type_) {
    case AutofillSnackbarType::kVirtualCard:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_NUMBER_SNACKBAR_MESSAGE_TEXT);
    case AutofillSnackbarType::kMandatoryReauth:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MANDATORY_REAUTH_SNACKBAR_MESSAGE_TEXT);
    case AutofillSnackbarType::kUnspecified:
      NOTREACHED_NORETURN();
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
    case AutofillSnackbarType::kUnspecified:
      NOTREACHED_NORETURN();
  }
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
    case AutofillSnackbarType::kUnspecified:
      return "Unspecified";
  }
}

}  // namespace autofill
