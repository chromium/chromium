// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_VIEW_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model_observer.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

class AuthenticatorRequestSheetView;

namespace autofill {

class WebauthnDialogController;
class WebauthnDialogModel;

class WebauthnDialogViewImpl : public WebauthnDialogView,
                               public WebauthnDialogModelObserver,
                               public views::DialogDelegateView {
 public:
  METADATA_HEADER(WebauthnDialogViewImpl);
  WebauthnDialogViewImpl(WebauthnDialogController* controller,
                         WebauthnDialogState dialog_state);
  WebauthnDialogViewImpl(const WebauthnDialogViewImpl&) = delete;
  WebauthnDialogViewImpl& operator=(const WebauthnDialogViewImpl&) = delete;
  ~WebauthnDialogViewImpl() override;

  // WebauthnDialogView:
  WebauthnDialogModel* GetDialogModel() const override;

  // WebauthnDialogModelObserver:
  void OnDialogStateChanged() override;

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  std::u16string GetWindowTitle() const override;

 private:
  // Closes the dialog.
  void Hide();

  // Re-inits dialog content and resizes.
  void RefreshContent();

  WebauthnDialogController* controller_ = nullptr;

  AuthenticatorRequestSheetView* sheet_view_ = nullptr;

  // Dialog model owned by |sheet_view_|. Since this dialog owns the
  // |sheet_view_|, the model_ will always be valid.
  WebauthnDialogModel* model_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_VIEW_IMPL_H_
