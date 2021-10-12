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
  if (autofill_progress_dialog_view_) {
    autofill_progress_dialog_view_->Dismiss(
        /*show_confirmation_before_closing=*/false);
    autofill_progress_dialog_view_ = nullptr;
  }
}

void AutofillProgressDialogControllerImpl::ShowDialog(
    base::OnceClosure cancel_callback) {
  DCHECK(!autofill_progress_dialog_view_);

  cancel_callback_ = std::move(cancel_callback);
  autofill_progress_dialog_view_ =
      AutofillProgressDialogView::CreateAndShow(this);
}

void AutofillProgressDialogControllerImpl::DismissDialog(
    bool show_confirmation_before_closing) {
  if (!autofill_progress_dialog_view_)
    return;

  autofill_progress_dialog_view_->Dismiss(show_confirmation_before_closing);
  autofill_progress_dialog_view_ = nullptr;
}

void AutofillProgressDialogControllerImpl::OnDismissed() {
  // If the |autofill_progress_dialog_view_| is not a nullptr. It means the
  // dismissal was triggered by the user cancelling the flow. Thus we should
  // invoke the |cancel_callback_|.
  if (autofill_progress_dialog_view_) {
    autofill_progress_dialog_view_ = nullptr;
    std::move(cancel_callback_).Run();
    // TODO(crbug.com/1243475): Add metrics.
    return;
  }

  // Otherwise it was triggered by the backend components. The
  // |cancel_callback_| will not be invoked but be reset.
  cancel_callback_.Reset();
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

}  // namespace autofill
