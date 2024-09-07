// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/window/dialog_delegate.h"

class AuthenticatorRequestSheetView;

namespace autofill {

class WebauthnDialogController;
class WebauthnDialogModel;

class WebauthnDialogView : public WebauthnDialog,
                           public WebauthnDialogModelObserver,
                           public views::DialogDelegateView {
  METADATA_HEADER(WebauthnDialogView, views::DialogDelegateView)

 public:
  WebauthnDialogView(WebauthnDialogController* controller,
                     WebauthnDialogState dialog_state);
  WebauthnDialogView(const WebauthnDialogView&) = delete;
  WebauthnDialogView& operator=(const WebauthnDialogView&) = delete;
  ~WebauthnDialogView() override;

  // WebauthnDialog:
  WebauthnDialogModel* GetDialogModel() const override;

  // WebauthnDialogModelObserver:
  void OnDialogStateChanged() override;

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  std::u16string GetWindowTitle() const override;

 private:
  // Closes the dialog.
  void Hide();

  // Re-inits dialog content and resizes.
  void RefreshContent();

  raw_ptr<WebauthnDialogController> controller_ = nullptr;

  raw_ptr<AuthenticatorRequestSheetView> sheet_view_ = nullptr;

  // Dialog model owned by |sheet_view_|. Since this dialog owns the
  // |sheet_view_|, the model_ will always be valid.
  raw_ptr<WebauthnDialogModel> model_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_VIEW_H_
