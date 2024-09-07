// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller_impl.h"

#include <string>

#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

class PaymentsWindowUserConsentDialogControllerImplTest : public testing::Test {
 public:
  PaymentsWindowUserConsentDialogControllerImplTest() {
    controller_ =
        std::make_unique<PaymentsWindowUserConsentDialogControllerImpl>(
            accept_callback_.Get(), cancel_callback_.Get());
  }

  ~PaymentsWindowUserConsentDialogControllerImplTest() override = default;

  PaymentsWindowUserConsentDialogControllerImpl* controller() {
    return controller_.get();
  }

 protected:
  base::MockOnceClosure accept_callback_;
  base::MockOnceClosure cancel_callback_;

 private:
  std::unique_ptr<PaymentsWindowUserConsentDialogControllerImpl> controller_;
};

// Tests that the correct strings are returned for the dialog.
TEST_F(PaymentsWindowUserConsentDialogControllerImplTest,
       CorrectStringsAreReturned) {
  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_LOADING_AND_CONSENT_DIALOG_TITLE_VCN_3DS));
  EXPECT_EQ(
      controller()->GetDialogDescription(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_DESCRIPTION_VCN_3DS));
  EXPECT_EQ(
      controller()->GetOkButtonLabel(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_OK_BUTTON_LABEL_VCN_3DS));
}

// Tests that the accept callback is triggered when the dialog is accepted.
TEST_F(PaymentsWindowUserConsentDialogControllerImplTest,
       AcceptCallbackTriggeredOnDialogAcceptance) {
  EXPECT_CALL(accept_callback_, Run);
  controller()->OnOkButtonClicked();
}

// Tests that the cancel callback is triggered when the dialog is cancelled.
TEST_F(PaymentsWindowUserConsentDialogControllerImplTest,
       CancelCallbackTriggeredOnDialogCancelling) {
  EXPECT_CALL(cancel_callback_, Run);
  controller()->OnCancelButtonClicked();
}

}  // namespace autofill::payments
