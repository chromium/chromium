// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/payments/payment_credential_enrollment_dialog_view.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/payments/content/payment_credential_enrollment_model.h"
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

const SkBitmap CreateInstrumentIcon(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kInstrumentIconWidth, kInstrumentIconHeight);
  bitmap.eraseColor(color);
  return bitmap;
}

}  // namespace

class PaymentCredentialEnrollmentDialogViewTest
    : public InProcessBrowserTest,
      public PaymentCredentialEnrollmentDialogView::ObserverForTest {
 public:
  enum DialogEvent : int {
    DIALOG_OPENED,
    DIALOG_CLOSED,
  };

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void CreateModel() {
    model_.set_title(
        l10n_util::GetStringUTF16(IDS_PAYMENT_CREDENTIAL_ENROLLMENT_TITLE));

    model_.set_description(l10n_util::GetStringUTF16(
        IDS_PAYMENT_CREDENTIAL_ENROLLMENT_DESCRIPTION));

    std::unique_ptr<SkBitmap> instrument_icon =
        std::make_unique<SkBitmap>(CreateInstrumentIcon(SK_ColorBLUE));
    instrument_icon_ = instrument_icon.get();
    model_.set_instrument_icon(std::move(instrument_icon));

    model_.set_instrument_name(u"Visa ••••4444");

    model_.set_accept_button_label(l10n_util::GetStringUTF16(
        IDS_PAYMENT_CREDENTIAL_ENROLLMENT_ACCEPT_BUTTON_LABEL));
    model_.set_cancel_button_label(l10n_util::GetStringUTF16(
        IDS_PAYMENT_CREDENTIAL_ENROLLMENT_CANCEL_BUTTON_LABEL));
  }

  void InvokePaymentCredentialEnrollmentUI() {
    content::WebContents* web_contents = GetActiveWebContents();

    // The PaymentCredentialEnrollmentDialogView object is memory managed by the
    // views:: machinery, so this test just holds a WeakPtr.
    dialog_view_ = (new PaymentCredentialEnrollmentDialogView(
                        /*observer_for_test=*/this))
                       ->GetWeakPtr();

    ResetEventWaiter(DialogEvent::DIALOG_OPENED);
    dialog_view_->ShowDialog(
        web_contents, model_.GetWeakPtr(),
        base::BindOnce(
            &PaymentCredentialEnrollmentDialogViewTest::AcceptCallback,
            base::Unretained(this)),
        base::BindOnce(
            &PaymentCredentialEnrollmentDialogViewTest::CancelCallback,
            base::Unretained(this)));
    event_waiter_->Wait();

    // The web-modal dialog should be open.
    web_modal::WebContentsModalDialogManager*
        web_contents_modal_dialog_manager =
            web_modal::WebContentsModalDialogManager::FromWebContents(
                web_contents);
    EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());
  }

  void ExpectLabelText(
      const std::u16string& text,
      PaymentCredentialEnrollmentDialogView::DialogViewID view_id) {
    EXPECT_EQ(text, static_cast<views::Label*>(
                        dialog_view_->GetViewByID(static_cast<int>(view_id)))
                        ->GetText());
  }

  void ExpectViewMatchesModel() {
    ASSERT_NE(dialog_view_, nullptr);

    EXPECT_EQ(model_.accept_button_label(),
              dialog_view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK));

    EXPECT_EQ(model_.cancel_button_label(),
              dialog_view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));

    EXPECT_TRUE(dialog_view_->GetViewByID(static_cast<int>(
        PaymentCredentialEnrollmentDialogView::DialogViewID::HEADER_ICON)));

    EXPECT_EQ(model_.progress_bar_visible(),
              dialog_view_
                  ->GetViewByID(
                      static_cast<int>(PaymentCredentialEnrollmentDialogView::
                                           DialogViewID::PROGRESS_BAR))
                  ->GetVisible());

    ExpectLabelText(model_.title(),
                    PaymentCredentialEnrollmentDialogView::DialogViewID::TITLE);

    ExpectLabelText(
        model_.description(),
        PaymentCredentialEnrollmentDialogView::DialogViewID::DESCRIPTION);

    ASSERT_EQ(instrument_icon_, model_.instrument_icon());
    EXPECT_TRUE(cc::MatchesBitmap(
        *model_.instrument_icon(),
        *(static_cast<views::ImageView*>(
              dialog_view_->GetViewByID(
                  static_cast<int>(PaymentCredentialEnrollmentDialogView::
                                       DialogViewID::INSTRUMENT_ICON)))
              ->GetImage()
              .bitmap()),
        cc::ExactPixelComparator(/*discard_alpha=*/false)));

    ExpectLabelText(
        model_.instrument_name(),
        PaymentCredentialEnrollmentDialogView::DialogViewID::INSTRUMENT_NAME);

    if (!model_.extra_description().empty()) {
      ExpectLabelText(model_.extra_description(),
                      PaymentCredentialEnrollmentDialogView::DialogViewID::
                          EXTRA_DESCRIPTION);
    } else {
      EXPECT_EQ(nullptr, dialog_view_->GetViewByID(static_cast<int>(
                             PaymentCredentialEnrollmentDialogView::
                                 DialogViewID::EXTRA_DESCRIPTION)));
    }
  }

  void ClickAcceptAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    dialog_view_->AcceptDialog();
    event_waiter_->Wait();

    // Expect accept button pressed and accept callback called
    EXPECT_TRUE(accept_pressed_);
    EXPECT_FALSE(cancel_pressed_);
    EXPECT_TRUE(accept_called_);
    EXPECT_FALSE(cancel_called_);
  }

  void ClickCancelAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    dialog_view_->CancelDialog();
    event_waiter_->Wait();

    // Expect cancel button pressed and cancel callback called
    EXPECT_TRUE(cancel_pressed_);
    EXPECT_FALSE(accept_pressed_);
    EXPECT_TRUE(cancel_called_);
    EXPECT_FALSE(accept_called_);
  }

  void CloseDialogAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    dialog_view_->HideDialog();
    event_waiter_->Wait();

    // Expect no button pressed and cancel callback called
    EXPECT_FALSE(cancel_pressed_);
    EXPECT_FALSE(accept_pressed_);
    EXPECT_TRUE(cancel_called_);
    EXPECT_FALSE(accept_called_);
  }

  void ResetEventWaiter(DialogEvent event) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<DialogEvent>>(
        std::list<DialogEvent>{event});
  }

  void AcceptCallback() { accept_called_ = true; }

  void CancelCallback() { cancel_called_ = true; }

  // PaymentCredentialEnrollmentDialogView::ObserverForTest:
  void OnDialogOpened() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::DIALOG_OPENED);
  }

  void OnDialogClosed() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::DIALOG_CLOSED);
  }

  void OnAcceptButtonPressed() override { accept_pressed_ = true; }

  void OnCancelButtonPressed() override { cancel_pressed_ = true; }

 protected:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;

  PaymentCredentialEnrollmentModel model_;
  base::WeakPtr<PaymentCredentialEnrollmentDialogView> dialog_view_;

  SkBitmap* instrument_icon_ = nullptr;

  bool accept_called_ = false;
  bool cancel_called_ = false;

  bool accept_pressed_ = false;
  bool cancel_pressed_ = false;
};

IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       AcceptButtonTest) {
  CreateModel();

  InvokePaymentCredentialEnrollmentUI();

  ExpectViewMatchesModel();

  ClickAcceptAndWait();
}

IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       CancelButtonTest) {
  CreateModel();

  InvokePaymentCredentialEnrollmentUI();

  ExpectViewMatchesModel();

  ClickCancelAndWait();
}

IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       CloseDialogTest) {
  CreateModel();

  InvokePaymentCredentialEnrollmentUI();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       ProgressBarVisible) {
  CreateModel();
  model_.set_progress_bar_visible(true);

  InvokePaymentCredentialEnrollmentUI();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       ShowProgressBar) {
  CreateModel();

  ASSERT_FALSE(model_.progress_bar_visible());

  InvokePaymentCredentialEnrollmentUI();

  ExpectViewMatchesModel();

  model_.set_progress_bar_visible(true);
  model_.set_accept_button_enabled(false);
  model_.set_cancel_button_enabled(false);
  dialog_view_->OnModelUpdated();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       OnModelUpdated) {
  CreateModel();

  InvokePaymentCredentialEnrollmentUI();

  ExpectViewMatchesModel();

  model_.set_title(u"Test Title");
  model_.set_description(u"Test description");
  model_.set_instrument_name(u"Test instrument");
  model_.set_accept_button_label(u"Test accept");
  model_.set_cancel_button_label(u"Test cancel");

  dialog_view_->OnModelUpdated();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

// Test the two reasons an instrument icon is updated: The model's bitmap
// pointer changed, or the bitmap itself changed.
IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       InstrumentIconUpdated) {
  CreateModel();

  InvokePaymentCredentialEnrollmentUI();

  ExpectViewMatchesModel();

  // Change the bitmap pointer
  std::unique_ptr<SkBitmap> instrument_icon =
      std::make_unique<SkBitmap>(CreateInstrumentIcon(SK_ColorGREEN));
  instrument_icon_ = instrument_icon.get();
  model_.set_instrument_icon(std::move(instrument_icon));
  dialog_view_->OnModelUpdated();
  ExpectViewMatchesModel();

  // Change the bitmap itself without touching the model's pointer
  *instrument_icon_ = CreateInstrumentIcon(SK_ColorRED);
  dialog_view_->OnModelUpdated();
  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       ExtraIncognitoDescription) {
  CreateModel();

  model_.set_extra_description(l10n_util::GetStringUTF16(
      IDS_PAYMENT_CREDENTIAL_ENROLLMENT_OFF_THE_RECORD_DESCRIPTION));

  InvokePaymentCredentialEnrollmentUI();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(PaymentCredentialEnrollmentDialogViewTest,
                       WebContentsClosed) {
  CreateModel();

  InvokePaymentCredentialEnrollmentUI();

  // Test passes if there is no crash.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  GetActiveWebContents()->Close();
  event_waiter_->Wait();
}

}  // namespace payments
