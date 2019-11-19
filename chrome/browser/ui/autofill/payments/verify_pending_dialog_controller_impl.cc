// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/verify_pending_dialog_controller_impl.h"

#include "chrome/browser/ui/autofill/payments/verify_pending_dialog_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

VerifyPendingDialogControllerImpl::VerifyPendingDialogControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

VerifyPendingDialogControllerImpl::~VerifyPendingDialogControllerImpl() {
  // If browser tab is closed when dialog is visible, the controller is
  // destroyed before the view is, so need to reset view's reference to
  // controller.
  if (dialog_view_)
    dialog_view_->Hide();
}

void VerifyPendingDialogControllerImpl::ShowDialog(
    base::OnceClosure cancel_card_verification_callback) {
  DCHECK(!dialog_view_);

  cancel_card_verification_callback_ =
      std::move(cancel_card_verification_callback);
  dialog_view_ =
      VerifyPendingDialogView::CreateDialogAndShow(this, web_contents());
}

void VerifyPendingDialogControllerImpl::OnCardVerificationCompleted() {
  if (!dialog_view_)
    return;

  cancel_card_verification_callback_.Reset();
  dialog_view_->Hide();
}

base::string16 VerifyPendingDialogControllerImpl::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_VERIFY_PENDING_DIALOG_TITLE);
}

void VerifyPendingDialogControllerImpl::OnCancel() {
  if (cancel_card_verification_callback_)
    std::move(cancel_card_verification_callback_).Run();
}

void VerifyPendingDialogControllerImpl::OnDialogClosed() {
  dialog_view_ = nullptr;
  cancel_card_verification_callback_.Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VerifyPendingDialogControllerImpl)

}  // namespace autofill
