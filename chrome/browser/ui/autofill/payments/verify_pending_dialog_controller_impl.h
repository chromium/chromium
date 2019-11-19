// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/verify_pending_dialog_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class VerifyPendingDialogView;

// Implementation of the per-tab controller to control the
// VerifyPendingDialogView. Lazily initialized when used.
class VerifyPendingDialogControllerImpl
    : public VerifyPendingDialogController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<VerifyPendingDialogControllerImpl> {
 public:
  ~VerifyPendingDialogControllerImpl() override;

  void ShowDialog(base::OnceClosure cancel_card_verification_callback);

  // Close the dialog when card verification is completed.
  void OnCardVerificationCompleted();

  // VerifyPendingDialogController:
  base::string16 GetDialogTitle() const override;
  void OnCancel() override;
  void OnDialogClosed() override;

  VerifyPendingDialogView* dialog_view() { return dialog_view_; }

 protected:
  explicit VerifyPendingDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<VerifyPendingDialogControllerImpl>;

  // Callback invoked when the cancel button in the dialog is clicked. Will
  // cancel the card verification in progress.
  base::OnceClosure cancel_card_verification_callback_;

  VerifyPendingDialogView* dialog_view_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(VerifyPendingDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VERIFY_PENDING_DIALOG_CONTROLLER_IMPL_H_
