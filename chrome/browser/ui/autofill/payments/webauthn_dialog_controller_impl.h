// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "content/public/browser/page_user_data.h"

namespace autofill {

class WebauthnDialogModel;
class WebauthnDialog;
enum class WebauthnDialogState;

// Implementation of the per-outermost primary page controller to control the
// WebauthnDialog. Lazily initialized when used.
class WebauthnDialogControllerImpl
    : public WebauthnDialogController,
      public content::PageUserData<WebauthnDialogControllerImpl> {
 public:
  WebauthnDialogControllerImpl(const WebauthnDialogControllerImpl&) = delete;
  WebauthnDialogControllerImpl& operator=(const WebauthnDialogControllerImpl&) =
      delete;
  ~WebauthnDialogControllerImpl() override;

  void ShowOfferDialog(payments::PaymentsAutofillClient::WebauthnDialogCallback
                           offer_dialog_callback);
  void ShowVerifyPendingDialog(
      payments::PaymentsAutofillClient::WebauthnDialogCallback
          verify_pending_dialog_callback);
  bool CloseDialog();
  void UpdateDialog(WebauthnDialogState dialog_state);

  // WebauthnDialogController:
  void OnOkButtonClicked() override;
  void OnCancelButtonClicked() override;
  void OnDialogClosed() override;
  content::WebContents* GetWebContents() override;

  WebauthnDialog* dialog() { return dialog_; }

 protected:
  explicit WebauthnDialogControllerImpl(content::Page& page);

 private:
  friend class content::PageUserData<WebauthnDialogControllerImpl>;

  // Clicking either the OK button or the cancel button in the dialog
  // will invoke this repeating callback. Note this repeating callback can
  // be run twice, since after the accept button in the offer dialog is
  // clicked, the dialog stays and the cancel button is still clickable.
  payments::PaymentsAutofillClient::WebauthnDialogCallback callback_;

  raw_ptr<WebauthnDialogModel> dialog_model_ = nullptr;
  raw_ptr<WebauthnDialog> dialog_ = nullptr;

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_
