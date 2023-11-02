// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller_impl.h"

#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_state.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_view.h"
#include "components/autofill/core/browser/payments/webauthn_callback_types.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

WebauthnDialogControllerImpl::WebauthnDialogControllerImpl(content::Page& page)
    : content::PageUserData<WebauthnDialogControllerImpl>(page) {
  // WebauthnDialogControllerImpl is only for the outermost primary page.
  DCHECK(page.IsPrimary());
}

WebauthnDialogControllerImpl::~WebauthnDialogControllerImpl() {
  // This part of code is executed only if browser window is closed when the
  // dialog is visible. In this case the controller is destroyed before
  // WebauthnDialogViewImpl::dtor() being called, but the reference to
  // controller is not reset. Need to reset via WebauthnDialogViewImpl::Hide()
  // to avoid crash.
  if (dialog_model_)
    dialog_model_->SetDialogState(WebauthnDialogState::kInactive);
}

void WebauthnDialogControllerImpl::ShowOfferDialog(
    AutofillClient::WebauthnDialogCallback offer_dialog_callback) {
  DCHECK(!dialog_model_);

  callback_ = std::move(offer_dialog_callback);
  dialog_view_ =
      WebauthnDialogView::CreateAndShow(this, WebauthnDialogState::kOffer);
  dialog_model_ = dialog_view_->GetDialogModel();
}

void WebauthnDialogControllerImpl::ShowVerifyPendingDialog(
    AutofillClient::WebauthnDialogCallback verify_pending_dialog_callback) {
  DCHECK(!dialog_model_);

  callback_ = std::move(verify_pending_dialog_callback);
  dialog_view_ = WebauthnDialogView::CreateAndShow(
      this, WebauthnDialogState::kVerifyPending);
  dialog_model_ = dialog_view_->GetDialogModel();
}

bool WebauthnDialogControllerImpl::CloseDialog() {
  if (!dialog_model_)
    return false;

  dialog_model_->SetDialogState(WebauthnDialogState::kInactive);
  return true;
}

void WebauthnDialogControllerImpl::UpdateDialog(
    WebauthnDialogState dialog_state) {
  dialog_model_->SetDialogState(dialog_state);
  // TODO(crbug.com/991037): Handle callback resetting for verify pending
  // dialog. Right now this function should only be passed in
  // WebauthnDialogState::kOfferError.
  DCHECK_EQ(dialog_state, WebauthnDialogState::kOfferError);
  callback_.Reset();
}

void WebauthnDialogControllerImpl::OnDialogClosed() {
  dialog_model_ = nullptr;
  dialog_view_ = nullptr;
  callback_.Reset();
}

content::WebContents* WebauthnDialogControllerImpl::GetWebContents() {
  return content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
}

void WebauthnDialogControllerImpl::OnOkButtonClicked() {
  // The OK button is available only when the dialog is in
  // WebauthnDialogState::kOffer state.
  DCHECK(callback_);
  callback_.Run(WebauthnDialogCallbackType::kOfferAccepted);
  dialog_model_->SetDialogState(WebauthnDialogState::kOfferPending);
}

void WebauthnDialogControllerImpl::OnCancelButtonClicked() {
  switch (dialog_model_->dialog_state()) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
      DCHECK(callback_);
      callback_.Run(WebauthnDialogCallbackType::kOfferCancelled);
      return;
    case WebauthnDialogState::kVerifyPending:
      DCHECK(callback_);
      callback_.Run(WebauthnDialogCallbackType::kVerificationCancelled);
      return;
    case WebauthnDialogState::kUnknown:
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kOfferError:
      NOTREACHED();
      return;
  }
}

PAGE_USER_DATA_KEY_IMPL(WebauthnDialogControllerImpl);

}  // namespace autofill
