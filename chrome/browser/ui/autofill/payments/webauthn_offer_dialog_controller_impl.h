// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class WebauthnOfferDialogModel;
class WebauthnOfferDialogView;

// Implementation of the per-tab controller to control the
// WebauthnOfferDialogView. Lazily initialized when used.
class WebauthnOfferDialogControllerImpl
    : public WebauthnOfferDialogController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<WebauthnOfferDialogControllerImpl> {
 public:
  ~WebauthnOfferDialogControllerImpl() override;

  void ShowOfferDialog(AutofillClient::WebauthnOfferDialogCallback callback);
  bool CloseDialog();
  void UpdateDialogWithError();

  // WebauthnOfferDialogController:
  void OnOkButtonClicked() override;
  void OnCancelButtonClicked() override;
  void OnDialogClosed() override;
  content::WebContents* GetWebContents() override;

  WebauthnOfferDialogView* dialog_view() { return dialog_view_; }

 protected:
  explicit WebauthnOfferDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<WebauthnOfferDialogControllerImpl>;

  // Callback invoked when any button in the dialog is clicked. Note this
  // repeating callback can be run twice, since after the accept button is
  // clicked, the dialog stays and the cancel button is still clickable.
  AutofillClient::WebauthnOfferDialogCallback offer_dialog_callback_;

  WebauthnOfferDialogModel* dialog_model_ = nullptr;
  WebauthnOfferDialogView* dialog_view_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(WebauthnOfferDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_IMPL_H_
