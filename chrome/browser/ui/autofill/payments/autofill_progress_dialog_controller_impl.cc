// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller_impl.h"

#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillProgressDialogControllerImpl::AutofillProgressDialogControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AutofillProgressDialogControllerImpl::~AutofillProgressDialogControllerImpl() {
  Dismiss();
}

void AutofillProgressDialogControllerImpl::ShowDialog() {
  DCHECK(!autofill_progress_dialog_view_);

  autofill_progress_dialog_view_ =
      AutofillProgressDialogView::CreateAndShow(this);
}

void AutofillProgressDialogControllerImpl::ShowConfirmation() {
  DCHECK(autofill_progress_dialog_view_);
  autofill_progress_dialog_view_->ShowConfirmation();
}

void AutofillProgressDialogControllerImpl::OnDismissed() {
  autofill_progress_dialog_view_ = nullptr;
}

const std::u16string AutofillProgressDialogControllerImpl::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE_V2);
}

const std::u16string
AutofillProgressDialogControllerImpl::GetCancelButtonLabel() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_CANCEL_BUTTON_LABEL);
}

const std::u16string AutofillProgressDialogControllerImpl::GetLoadingMessage() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_PROGRESS_BAR_MESSAGE);
}

const std::u16string
AutofillProgressDialogControllerImpl::GetConfirmationMessage() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_CONFIRMATION_MESSAGE);
}

content::WebContents* AutofillProgressDialogControllerImpl::GetWebContents() {
  return web_contents_;
}

void AutofillProgressDialogControllerImpl::Dismiss() {
  if (autofill_progress_dialog_view_)
    autofill_progress_dialog_view_->Dismiss();
}

}  // namespace autofill
