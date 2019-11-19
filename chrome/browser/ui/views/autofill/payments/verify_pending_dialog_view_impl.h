// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_VIEW_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/verify_pending_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

class VerifyPendingDialogController;

// The Views implementation of the dialog that shows the card verification is in
// progress. It is shown when card verification process starts only if WebAuthn
// has been enabled and opted in.
class VerifyPendingDialogViewImpl : public VerifyPendingDialogView,
                                    public views::DialogDelegateView {
 public:
  explicit VerifyPendingDialogViewImpl(
      VerifyPendingDialogController* controller);
  ~VerifyPendingDialogViewImpl() override;

  // VerifyPendingDialogView:
  void Hide() override;

  // views::DialogDelegateView:
  void AddedToWidget() override;
  bool Cancel() override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

 private:
  VerifyPendingDialogController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VerifyPendingDialogViewImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_VIEW_IMPL_H_
