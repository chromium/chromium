// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller_impl.h"

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_model.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_view.h"

namespace autofill {

WebauthnOfferDialogControllerImpl::WebauthnOfferDialogControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebauthnOfferDialogControllerImpl::~WebauthnOfferDialogControllerImpl() {
  // This part of code is executed only if browser window is closed when the
  // dialog is visible. In this case the controller is destroyed before
  // WebauthnOfferDialogViewImpl::dtor() being called, but the reference to
  // controller is not reset. Need to reset via
  // WebauthnOfferDialogViewImpl::Hide() to avoid crash.
  if (dialog_model_) {
    dialog_model_->SetDialogState(
        WebauthnOfferDialogModel::DialogState::kInactive);
  }
}

void WebauthnOfferDialogControllerImpl::ShowOfferDialog(
    AutofillClient::WebauthnOfferDialogCallback callback) {
  DCHECK(!dialog_model_);

  offer_dialog_callback_ = std::move(callback);
  dialog_view_ = WebauthnOfferDialogView::CreateAndShow(this);
  dialog_model_ = dialog_view_->GetDialogModel();
}

bool WebauthnOfferDialogControllerImpl::CloseDialog() {
  if (!dialog_model_)
    return false;

  dialog_model_->SetDialogState(
      WebauthnOfferDialogModel::DialogState::kInactive);
  return true;
}

void WebauthnOfferDialogControllerImpl::UpdateDialogWithError() {
  dialog_model_->SetDialogState(WebauthnOfferDialogModel::DialogState::kError);
  offer_dialog_callback_.Reset();
}

void WebauthnOfferDialogControllerImpl::OnDialogClosed() {
  dialog_model_ = nullptr;
  dialog_view_ = nullptr;
  offer_dialog_callback_.Reset();
}

content::WebContents* WebauthnOfferDialogControllerImpl::GetWebContents() {
  return web_contents();
}

void WebauthnOfferDialogControllerImpl::OnOkButtonClicked() {
  DCHECK(offer_dialog_callback_);
  offer_dialog_callback_.Run(/*did_accept=*/true);
  dialog_model_->SetDialogState(
      WebauthnOfferDialogModel::DialogState::kPending);
}

void WebauthnOfferDialogControllerImpl::OnCancelButtonClicked() {
  DCHECK(offer_dialog_callback_);
  offer_dialog_callback_.Run(/*did_accept=*/false);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebauthnOfferDialogControllerImpl)

}  // namespace autofill
