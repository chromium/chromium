// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller_impl.h"

#include <string>
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
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

void AutofillSnackbarControllerImpl::Show() {
  if (!autofill_snackbar_view_) {
    autofill_snackbar_view_ = AutofillSnackbarView::Create(this);
  }
  autofill_snackbar_view_->Show();
  base::UmaHistogramBoolean("Autofill.Snackbar.VirtualCard.Shown", true);
}

void AutofillSnackbarControllerImpl::Dismiss() {
  if (!autofill_snackbar_view_)
    return;
  autofill_snackbar_view_->Dismiss();
}

void AutofillSnackbarControllerImpl::SetViewForTesting(
    AutofillSnackbarView* view) {
  autofill_snackbar_view_ = view;
}

void AutofillSnackbarControllerImpl::OnActionClicked() {
  ManualFillingControllerImpl::GetOrCreate(web_contents_)
      ->ShowAccessorySheetTab(autofill::AccessoryTabType::CREDIT_CARDS);
  base::UmaHistogramBoolean("Autofill.Snackbar.VirtualCard.ActionClicked",
                            true);
}

void AutofillSnackbarControllerImpl::OnDismissed() {
  autofill_snackbar_view_ = nullptr;
}

std::u16string AutofillSnackbarControllerImpl::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_NUMBER_SNACKBAR_MESSAGE_TEXT);
}

std::u16string AutofillSnackbarControllerImpl::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_NUMBER_SNACKBAR_ACTION_TEXT);
}

content::WebContents* AutofillSnackbarControllerImpl::GetWebContents() const {
  return web_contents_;
}

}  // namespace autofill
