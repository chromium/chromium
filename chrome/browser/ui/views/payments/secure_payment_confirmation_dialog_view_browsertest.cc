// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"
#include "chrome/browser/ui/views/payments/test_secure_payment_confirmation_payment_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace payments {
namespace {

constexpr int kInstrumentIconWidth = 32;
constexpr int kInstrumentIconHeight = 20;

const SkBitmap CreateInstrumentIcon(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kInstrumentIconWidth, kInstrumentIconHeight);
  bitmap.eraseColor(color);
  return bitmap;
}

}  // namespace

class SecurePaymentConfirmationDialogViewTest
    : public InProcessBrowserTest,
      public SecurePaymentConfirmationDialogView::ObserverForTest {
 public:
  enum DialogEvent : int {
    DIALOG_OPENED,
    DIALOG_CLOSED,
  };

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void CreateModel() {
    model_.set_title(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_PURCHASE));

    model_.set_merchant_label(
        l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_STORE_LABEL));
    model_.set_merchant_value(base::UTF8ToUTF16("merchant.com"));

    model_.set_instrument_label(l10n_util::GetStringUTF16(
        IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME));
    model_.set_instrument_value(base::UTF8ToUTF16("Mastercard ****4444"));
    instrument_icon_ =
        std::make_unique<SkBitmap>(CreateInstrumentIcon(SK_ColorBLUE));
    model_.set_instrument_icon(instrument_icon_.get());

    model_.set_total_label(
        l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_TOTAL_LABEL));
    model_.set_total_value(base::UTF8ToUTF16("$20.00 USD"));

    model_.set_verify_button_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_BUTTON_LABEL));
    model_.set_cancel_button_label(l10n_util::GetStringUTF16(IDS_CANCEL));
  }

  void InvokeSecurePaymentConfirmationUI() {
    content::WebContents* web_contents = GetActiveWebContents();

    test_delegate_ =
        std::make_unique<TestSecurePaymentConfirmationPaymentRequestDelegate>(
            web_contents->GetMainFrame(), model_.GetWeakPtr(), this);

    ResetEventWaiter(DialogEvent::DIALOG_OPENED);
    test_delegate_->ShowDialog(nullptr);
    event_waiter_->Wait();

    // The web-modal dialog should be open.
    web_modal::WebContentsModalDialogManager*
        web_contents_modal_dialog_manager =
            web_modal::WebContentsModalDialogManager::FromWebContents(
                web_contents);
    EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());
  }

  void ExpectLabelText(
      const base::string16& text,
      SecurePaymentConfirmationDialogView::DialogViewID view_id) {
    EXPECT_EQ(text, static_cast<views::Label*>(
                        test_delegate_->dialog_view()->GetViewByID(
                            static_cast<int>(view_id)))
                        ->GetText());
  }

  void ExpectViewMatchesModel() {
    ASSERT_NE(test_delegate_->dialog_view(), nullptr);

    EXPECT_EQ(model_.verify_button_label(),
              test_delegate_->dialog_view()->GetDialogButtonLabel(
                  ui::DIALOG_BUTTON_OK));

    EXPECT_EQ(model_.cancel_button_label(),
              test_delegate_->dialog_view()->GetDialogButtonLabel(
                  ui::DIALOG_BUTTON_CANCEL));

    EXPECT_TRUE(test_delegate_->dialog_view()->GetViewByID(static_cast<int>(
        SecurePaymentConfirmationDialogView::DialogViewID::HEADER_ICON)));

    EXPECT_EQ(
        model_.progress_bar_visible(),
        test_delegate_->dialog_view()
            ->GetViewByID(static_cast<int>(SecurePaymentConfirmationDialogView::
                                               DialogViewID::PROGRESS_BAR))
            ->GetVisible());

    ExpectLabelText(model_.title(),
                    SecurePaymentConfirmationDialogView::DialogViewID::TITLE);

    ExpectLabelText(
        model_.merchant_label(),
        SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_LABEL);
    ExpectLabelText(
        model_.merchant_value(),
        SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);

    ExpectLabelText(
        model_.instrument_label(),
        SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_LABEL);
    ExpectLabelText(
        model_.instrument_value(),
        SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_VALUE);

    ASSERT_EQ(instrument_icon_.get(), model_.instrument_icon());
    EXPECT_TRUE(cc::MatchesBitmap(
        *model_.instrument_icon(),
        *(static_cast<views::ImageView*>(
              test_delegate_->dialog_view()->GetViewByID(
                  static_cast<int>(SecurePaymentConfirmationDialogView::
                                       DialogViewID::INSTRUMENT_ICON)))
              ->GetImage()
              .bitmap()),
        cc::ExactPixelComparator(/*discard_alpha=*/false)));

    ExpectLabelText(
        model_.total_label(),
        SecurePaymentConfirmationDialogView::DialogViewID::TOTAL_LABEL);
    ExpectLabelText(
        model_.total_value(),
        SecurePaymentConfirmationDialogView::DialogViewID::TOTAL_VALUE);
  }

  void ClickAcceptAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->dialog_view()->AcceptDialog();
    event_waiter_->Wait();

    EXPECT_TRUE(confirm_pressed_);
    EXPECT_FALSE(cancel_pressed_);
  }

  void ClickCancelAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->dialog_view()->CancelDialog();
    event_waiter_->Wait();

    EXPECT_TRUE(cancel_pressed_);
    EXPECT_FALSE(confirm_pressed_);
  }

  void CloseDialogAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->CloseDialog();
    event_waiter_->Wait();

    EXPECT_FALSE(cancel_pressed_);
    EXPECT_FALSE(confirm_pressed_);
  }

  void ResetEventWaiter(DialogEvent event) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<DialogEvent>>(
        std::list<DialogEvent>{event});
  }

  // SecurePaymentConfirmationDialogView::ObserverForTest:
  void OnDialogOpened() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::DIALOG_OPENED);
  }

  void OnDialogClosed() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::DIALOG_CLOSED);
  }

  void OnConfirmButtonPressed() override { confirm_pressed_ = true; }

  void OnCancelButtonPressed() override { cancel_pressed_ = true; }

 protected:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;

  SecurePaymentConfirmationModel model_;
  std::unique_ptr<TestSecurePaymentConfirmationPaymentRequestDelegate>
      test_delegate_;

  std::unique_ptr<SkBitmap> instrument_icon_;

  bool confirm_pressed_ = false;
  bool cancel_pressed_ = false;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       AcceptButtonTest) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  ClickAcceptAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       CancelButtonTest) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  ClickCancelAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       CloseDialogTest) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       ProgressBarVisible) {
  CreateModel();
  model_.set_progress_bar_visible(true);

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       ShowProgressBar) {
  CreateModel();

  ASSERT_FALSE(model_.progress_bar_visible());

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  model_.set_progress_bar_visible(true);
  test_delegate_->dialog_view()->OnModelUpdated();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       OnModelUpdated) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  model_.set_title(base::UTF8ToUTF16("Test Title"));
  model_.set_merchant_label(base::UTF8ToUTF16("Test merchant"));
  model_.set_merchant_value(base::UTF8ToUTF16("Test merchant value"));
  model_.set_instrument_label(base::UTF8ToUTF16("Test instrument"));
  model_.set_instrument_value(base::UTF8ToUTF16("Test instrument value"));
  model_.set_total_label(base::UTF8ToUTF16("Test total"));
  model_.set_total_value(base::UTF8ToUTF16("Test total value"));
  model_.set_verify_button_label(base::UTF8ToUTF16("Test verify"));
  model_.set_cancel_button_label(base::UTF8ToUTF16("Test cancel"));

  test_delegate_->dialog_view()->OnModelUpdated();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

// Test the two reasons an instrument icon is updated: The model's bitmap
// pointer changed, or the bitmap itself changed.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       InstrumentIconUpdated) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  // Change the bitmap pointer
  instrument_icon_ =
      std::make_unique<SkBitmap>(CreateInstrumentIcon(SK_ColorGREEN));
  model_.set_instrument_icon(instrument_icon_.get());
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();

  // Change the bitmap itself without touching the model's pointer
  *instrument_icon_ = CreateInstrumentIcon(SK_ColorRED);
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

}  // namespace payments
