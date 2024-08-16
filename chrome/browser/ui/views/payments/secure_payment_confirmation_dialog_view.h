// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/secure_payment_confirmation_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class StyledLabel;
}

namespace payments {

class PaymentUIObserver;

// Draws the user interface in the secure payment confirmation flow. Owned by
// the SecurePaymentConfirmationController.
class SecurePaymentConfirmationDialogView
    : public SecurePaymentConfirmationView,
      public views::DialogDelegateView {
  METADATA_HEADER(SecurePaymentConfirmationDialogView,
                  views::DialogDelegateView)

 public:
  class ObserverForTest {
   public:
    virtual void OnDialogClosed() = 0;
    virtual void OnConfirmButtonPressed() = 0;
    virtual void OnCancelButtonPressed() = 0;
    virtual void OnOptOutClicked() = 0;
  };

  // IDs that identify a view within the secure payment confirmation dialog.
  // Used to validate views in browsertests.
  enum class DialogViewID : int {
    VIEW_ID_NONE = 0,
    HEADER_ICON,
    TITLE,
    DESCRIPTION,
    MERCHANT_LABEL,
    MERCHANT_VALUE,
    INSTRUMENT_LABEL,
    INSTRUMENT_VALUE,
    INSTRUMENT_ICON,
    TOTAL_LABEL,
    TOTAL_VALUE,
    NETWORK_LABEL,
    NETWORK_VALUE,
    NETWORK_ICON,
    ISSUER_LABEL,
    ISSUER_VALUE,
    ISSUER_ICON
  };

  explicit SecurePaymentConfirmationDialogView(
      base::WeakPtr<ObserverForTest> observer_for_test,
      const base::WeakPtr<PaymentUIObserver> ui_observer_for_test);
  ~SecurePaymentConfirmationDialogView() override;

  // SecurePaymentConfirmationView:
  void ShowDialog(content::WebContents* web_contents,
                  base::WeakPtr<SecurePaymentConfirmationModel> model,
                  VerifyCallback verify_callback,
                  CancelCallback cancel_callback,
                  OptOutCallback opt_out_callback) override;
  void OnModelUpdated() override;
  void HideDialog() override;
  bool ClickOptOutForTesting() override;

  // views::DialogDelegate:
  bool ShouldShowCloseButton() const override;
  bool Accept() override;

  base::WeakPtr<SecurePaymentConfirmationDialogView> GetWeakPtr();

 private:
  void OnDialogAccepted();
  void OnDialogCancelled();
  void OnDialogClosed();
  void OnOptOutClicked();

  void InitChildViews();

  std::unique_ptr<views::View> CreateHeaderView();
  std::unique_ptr<views::View> CreateBodyView();
  std::unique_ptr<views::View> CreateRows();
  std::unique_ptr<views::View> CreateRowView(
      const std::u16string& label,
      DialogViewID label_id,
      const std::u16string& value,
      DialogViewID value_id,
      const SkBitmap* icon = nullptr,
      DialogViewID icon_id = DialogViewID::VIEW_ID_NONE);

  void UpdateLabelView(DialogViewID id, const std::u16string& text);

  base::WeakPtr<ObserverForTest> observer_for_test_;
  const base::WeakPtr<PaymentUIObserver> ui_observer_for_test_;

  VerifyCallback verify_callback_;
  CancelCallback cancel_callback_;
  OptOutCallback opt_out_callback_;

  // Cache the instrument icon pointer so we don't needlessly update it in
  // OnModelUpdated().
  raw_ptr<const SkBitmap, AcrossTasksDanglingUntriaged> instrument_icon_ =
      nullptr;
  // Cache the instrument icon generation ID to check if the instrument_icon_
  // has changed pixels.
  uint32_t instrument_icon_generation_id_ = 0;

  // The opt-out view stored in the dialog footnote. This is always created in
  // InitChildViews, but is only marked visible if opt-out was requested.
  raw_ptr<views::StyledLabel> opt_out_view_ = nullptr;

  base::WeakPtrFactory<SecurePaymentConfirmationDialogView> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_DIALOG_VIEW_H_
