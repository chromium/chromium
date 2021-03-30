// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_CREDENTIAL_ENROLLMENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_CREDENTIAL_ENROLLMENT_DIALOG_VIEW_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_credential_enrollment_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/window/dialog_delegate.h"

namespace payments {

// Draws the user interface in the payment credential enrollment flow.
class PaymentCredentialEnrollmentDialogView
    : public PaymentCredentialEnrollmentView,
      public views::DialogDelegateView {
 public:
  METADATA_HEADER(PaymentCredentialEnrollmentDialogView);

  class ObserverForTest {
   public:
    virtual void OnDialogOpened() = 0;
    virtual void OnDialogClosed() = 0;
    virtual void OnAcceptButtonPressed() = 0;
    virtual void OnCancelButtonPressed() = 0;
  };

  // IDs that identify a view within the secure payment confirmation dialog.
  // Used to validate views in browsertests.
  enum class DialogViewID : int {
    VIEW_ID_NONE = 0,
    HEADER_ICON,
    PROGRESS_BAR,
    TITLE,
    DESCRIPTION,
    INSTRUMENT_ICON,
    INSTRUMENT_NAME,
    EXTRA_DESCRIPTION
  };

  // The
  explicit PaymentCredentialEnrollmentDialogView(
      ObserverForTest* observer_for_test);
  ~PaymentCredentialEnrollmentDialogView() override;

  // PaymentCredentialEnrollmentView:
  void ShowDialog(content::WebContents* web_contents,
                  base::WeakPtr<PaymentCredentialEnrollmentModel> model,
                  AcceptCallback accept_callback,
                  CancelCallback cancel_callback) override;
  void OnModelUpdated() override;
  void HideDialog() override;

  // views::DialogDelegate:
  bool ShouldShowCloseButton() const override;
  bool Accept() override;

  base::WeakPtr<PaymentCredentialEnrollmentDialogView> GetWeakPtr();

 private:
  void OnDialogAccepted();
  void OnDialogCancelled();
  void OnDialogClosed();

  void InitChildViews();

  std::unique_ptr<views::View> CreateHeaderView();
  std::unique_ptr<views::View> CreateBodyView();
  std::unique_ptr<views::View> CreateDescription(const std::u16string& text,
                                                 DialogViewID view_id);
  std::unique_ptr<views::View> CreateInstrumentRow();

  void UpdateLabelView(DialogViewID id, const std::u16string& text);

  // May be null.
  ObserverForTest* observer_for_test_ = nullptr;

  AcceptCallback accept_callback_;
  CancelCallback cancel_callback_;

  // Cache the instrument icon pointer so we don't needlessly update it in
  // OnModelUpdated().
  const SkBitmap* instrument_icon_ = nullptr;
  // Cache the instrument icon generation ID to check if the instrument_icon_
  // has changed pixels.
  uint32_t instrument_icon_generation_id_ = 0;

  base::WeakPtrFactory<PaymentCredentialEnrollmentDialogView> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_CREDENTIAL_ENROLLMENT_DIALOG_VIEW_H_
