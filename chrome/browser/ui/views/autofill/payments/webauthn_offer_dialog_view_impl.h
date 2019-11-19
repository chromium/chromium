// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_model_observer.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"

class AuthenticatorRequestSheetView;

namespace autofill {

class WebauthnOfferDialogController;
class WebauthnOfferDialogModel;

// The view of the dialog that offers the option to use device's platform
// authenticator. It is shown automatically after card unmasked details are
// obtained and filled into the form.
class WebauthnOfferDialogViewImpl : public WebauthnOfferDialogView,
                                    public WebauthnOfferDialogModelObserver,
                                    public views::DialogDelegateView {
 public:
  explicit WebauthnOfferDialogViewImpl(
      WebauthnOfferDialogController* controller);
  ~WebauthnOfferDialogViewImpl() override;

  // WebauthnOfferDialogView:
  WebauthnOfferDialogModel* GetDialogModel() const override;

  // WebauthnOfferDialogModelObserver:
  void OnDialogStateChanged() override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;
  int GetDialogButtons() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;


 private:
  // Closes the dialog.
  void Hide();

  // Re-inits dialog content and resizes.
  void RefreshContent();

  WebauthnOfferDialogController* controller_ = nullptr;

  AuthenticatorRequestSheetView* sheet_view_ = nullptr;

  // Dialog model owned by |sheet_view_|. Since this dialog owns the
  // |sheet_view_|, the model_ will always be valid.
  WebauthnOfferDialogModel* model_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebauthnOfferDialogViewImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_IMPL_H_
