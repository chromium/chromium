// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "chrome/browser/ui/views/payments/test_secure_payment_confirmation_payment_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/payments/core/sizes.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"

namespace payments {
namespace {

const SkBitmap CreateInstrumentIcon(SkColor color, int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return bitmap;
}

const SkBitmap CreateMaxSizeInstrumentIcon(SkColor color) {
  return CreateInstrumentIcon(
      color, kSecurePaymentConfirmationInstrumentIconMaximumWidthPx,
      kSecurePaymentConfirmationInstrumentIconHeightPx);
}

}  // namespace

class SecurePaymentConfirmationDialogViewTest
    : public DialogBrowserTest,
      public SecurePaymentConfirmationDialogView::ObserverForTest {
 public:
  enum DialogEvent : int {
    DIALOG_OPENED,
    DIALOG_CLOSED,
    OPT_OUT_CLICKED,
  };

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents = GetActiveWebContents();
    CreateModel();

    test_delegate_ =
        std::make_unique<TestSecurePaymentConfirmationPaymentRequestDelegate>(
            web_contents->GetPrimaryMainFrame(), model_.GetWeakPtr(),
            GetWeakPtr());

    // TODO(crbug.com/1360854): Ideally, we'd expect the browser window to be
    // active here and could check that |IsBrowserWindowActivate()| returned
    // true, but on wayland, windows cannot be activated as they are on other
    // platforms.
    EXPECT_EQ(browser()->window()->IsActive(),
              test_delegate_->IsBrowserWindowActive());

    test_delegate_->ShowDialog(nullptr);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::WeakPtr<SecurePaymentConfirmationDialogViewTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void CreateModel() {
    model_.set_title(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_PURCHASE));

    model_.set_merchant_label(
        l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_STORE_LABEL));
    model_.set_merchant_name(absl::optional<std::u16string>(u"Test Merchant"));
    model_.set_merchant_origin(
        absl::optional<std::u16string>(u"merchant1.com"));

    model_.set_instrument_label(l10n_util::GetStringUTF16(
        IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME));
    model_.set_instrument_value(u"Mastercard ****4444");
    instrument_icon_ =
        std::make_unique<SkBitmap>(CreateMaxSizeInstrumentIcon(SK_ColorBLUE));
    model_.set_instrument_icon(instrument_icon_.get());

    model_.set_total_label(
        l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_TOTAL_LABEL));
    model_.set_total_value(u"$20.00 USD");

    model_.set_verify_button_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_BUTTON_LABEL));
    model_.set_cancel_button_label(l10n_util::GetStringUTF16(IDS_CANCEL));

    model_.set_opt_out_visible(false);
    model_.set_opt_out_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LABEL));
    model_.set_opt_out_link_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LINK_LABEL));
    model_.set_relying_party_id(u"relyingparty.com");
  }

  void InvokeSecurePaymentConfirmationUI() {
    content::WebContents* web_contents = GetActiveWebContents();

    test_delegate_ =
        std::make_unique<TestSecurePaymentConfirmationPaymentRequestDelegate>(
            web_contents->GetPrimaryMainFrame(), model_.GetWeakPtr(),
            GetWeakPtr());

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
      const std::u16string& text,
      SecurePaymentConfirmationDialogView::DialogViewID view_id) {
    EXPECT_EQ(text, static_cast<views::Label*>(
                        test_delegate_->dialog_view()->GetViewByID(
                            static_cast<int>(view_id)))
                        ->GetText());
  }

  void ExpectOptOutText(views::View* view,
                        const std::u16string& relying_party_id,
                        const std::u16string& opt_out_link_label) {
    // To avoid overfitting, we check only that the opt-out label contains both
    // the relying party and the call-to-action text that is expected.
    std::string opt_out_text =
        base::UTF16ToUTF8(static_cast<views::StyledLabel*>(view)->GetText());
    EXPECT_THAT(opt_out_text,
                ::testing::HasSubstr(base::UTF16ToUTF8(relying_party_id)));
    EXPECT_THAT(opt_out_text,
                ::testing::HasSubstr(base::UTF16ToUTF8(opt_out_link_label)));
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
        FormatMerchantLabel(model_.merchant_name(), model_.merchant_origin()),
        SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);

    ExpectLabelText(
        model_.instrument_label(),
        SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_LABEL);
    ExpectLabelText(
        model_.instrument_value(),
        SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_VALUE);

    ASSERT_EQ(instrument_icon_.get(), model_.instrument_icon());
    const SkBitmap* expected_icon =
        instrument_icon_->drawsNothing()
            ? gfx::CreateVectorIcon(
                  kCreditCardIcon,
                  kSecurePaymentConfirmationInstrumentIconDefaultWidthPx,
                  test_delegate_->dialog_view()->GetColorProvider()->GetColor(
                      ui::kColorDialogForeground))
                  .bitmap()
            : model_.instrument_icon();
    EXPECT_TRUE(cc::MatchesBitmap(
        *expected_icon,
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

    // The Opt Out link always exists, but should only be visible if requested.
    views::View* opt_out_view =
        test_delegate_->dialog_view()->GetFootnoteViewForTesting();
    EXPECT_NE(opt_out_view, nullptr);
    EXPECT_EQ(opt_out_view->GetVisible(), model_.opt_out_visible());
    ExpectOptOutText(opt_out_view, model_.relying_party_id(),
                     model_.opt_out_link_label());
  }

  void ClickAcceptAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->dialog_view()->AcceptDialog();
    event_waiter_->Wait();

    EXPECT_TRUE(confirm_pressed_);
    EXPECT_FALSE(cancel_pressed_);
    EXPECT_FALSE(opt_out_clicked_);

    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "AuthenticationDialogResult",
        1);
    histogram_tester_.ExpectBucketCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "AuthenticationDialogResult",
        SecurePaymentConfirmationAuthenticationDialogResult::kAccepted, 1);
  }

  void ClickCancelAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->dialog_view()->CancelDialog();
    event_waiter_->Wait();

    EXPECT_TRUE(cancel_pressed_);
    EXPECT_FALSE(confirm_pressed_);
    EXPECT_FALSE(opt_out_clicked_);

    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "AuthenticationDialogResult",
        1);
    histogram_tester_.ExpectBucketCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "AuthenticationDialogResult",
        SecurePaymentConfirmationAuthenticationDialogResult::kCanceled, 1);
  }

  void CloseDialogAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->CloseDialog();
    event_waiter_->Wait();

    EXPECT_FALSE(cancel_pressed_);
    EXPECT_FALSE(confirm_pressed_);
    EXPECT_FALSE(opt_out_clicked_);

    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "AuthenticationDialogResult",
        1);
    histogram_tester_.ExpectBucketCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "AuthenticationDialogResult",
        SecurePaymentConfirmationAuthenticationDialogResult::kClosed, 1);
  }

  void ClickOptOutAndWait() {
    ResetEventWaiter(DialogEvent::OPT_OUT_CLICKED);

    views::StyledLabel* opt_out_label = static_cast<views::StyledLabel*>(
        test_delegate_->dialog_view()->GetFootnoteViewForTesting());
    opt_out_label->ClickFirstLinkForTesting();

    event_waiter_->Wait();

    EXPECT_FALSE(cancel_pressed_);
    EXPECT_FALSE(confirm_pressed_);
    EXPECT_TRUE(opt_out_clicked_);

    histogram_tester_.ExpectTotalCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "AuthenticationDialogResult",
        1);
    histogram_tester_.ExpectBucketCount(
        "PaymentRequest.SecurePaymentConfirmation.Funnel."
        "AuthenticationDialogResult",
        SecurePaymentConfirmationAuthenticationDialogResult::kOptOut, 1);
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

  void OnOptOutClicked() override {
    opt_out_clicked_ = true;
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::OPT_OUT_CLICKED);
  }

 protected:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;

  SecurePaymentConfirmationModel model_;
  std::unique_ptr<TestSecurePaymentConfirmationPaymentRequestDelegate>
      test_delegate_;

  std::unique_ptr<SkBitmap> instrument_icon_;

  bool confirm_pressed_ = false;
  bool cancel_pressed_ = false;
  bool opt_out_clicked_ = false;

  base::HistogramTester histogram_tester_;

  base::WeakPtrFactory<SecurePaymentConfirmationDialogViewTest>
      weak_ptr_factory_{this};
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

  model_.set_title(u"Test Title");
  model_.set_merchant_label(u"Test merchant");
  model_.set_merchant_name(
      absl::optional<std::u16string>(u"Test merchant value"));
  model_.set_merchant_origin(absl::optional<std::u16string>(u"merchant2.com"));
  model_.set_instrument_label(u"Test instrument");
  model_.set_instrument_value(u"Test instrument value");
  model_.set_total_label(u"Test total");
  model_.set_total_value(u"Test total value");
  model_.set_verify_button_label(u"Test verify");
  model_.set_cancel_button_label(u"Test cancel");

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
      std::make_unique<SkBitmap>(CreateMaxSizeInstrumentIcon(SK_ColorGREEN));
  model_.set_instrument_icon(instrument_icon_.get());
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();

  // Change the bitmap itself without touching the model's pointer
  *instrument_icon_ = CreateMaxSizeInstrumentIcon(SK_ColorRED);
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       WebContentsClosed) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  // Test passes if there is no crash.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  GetActiveWebContents()->Close();
  event_waiter_->Wait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       DefaultInstrumentIcon) {
  CreateModel();

  instrument_icon_ = std::make_unique<SkBitmap>();
  ASSERT_TRUE(instrument_icon_->drawsNothing());
  model_.set_instrument_icon(instrument_icon_.get());

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       MerchantLabelFormat) {
  CreateModel();

  // Both merchant name and origin
  model_.set_merchant_name(absl::optional<std::u16string>(u"Test Merchant"));
  model_.set_merchant_origin(absl::optional<std::u16string>(u"merchant.com"));
  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();
  ExpectLabelText(
      u"Test Merchant (merchant.com)",
      SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);

  // Only merchant name, no origin
  model_.set_merchant_name(absl::optional<std::u16string>(u"Test Merchant 2"));
  model_.set_merchant_origin(absl::optional<std::u16string>());
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();
  ExpectLabelText(
      u"Test Merchant 2",
      SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);

  // Only merchant origin, no name
  model_.set_merchant_name(absl::optional<std::u16string>());
  model_.set_merchant_origin(absl::optional<std::u16string>(u"merchant2.com"));
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();
  ExpectLabelText(
      u"merchant2.com",
      SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);

  CloseDialogAndWait();
}

// Test that an oversized instrument icon is resized down to the maximum size.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       OversizedInstrumentIcon) {
  CreateModel();

  *instrument_icon_ = CreateInstrumentIcon(
      SK_ColorRED, kSecurePaymentConfirmationInstrumentIconMaximumWidthPx * 2,
      kSecurePaymentConfirmationInstrumentIconHeightPx * 2);

  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  views::ImageView* image_view = static_cast<views::ImageView*>(
      test_delegate_->dialog_view()->GetViewByID(static_cast<int>(
          SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_ICON)));
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconMaximumWidthPx,
            image_view->GetImageBounds().width());
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconHeightPx,
            image_view->GetImageBounds().height());

  CloseDialogAndWait();
}

// Test that an undersized instrument icon is resized up to the minimum size.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       UndersizedInstrumentIcon) {
  CreateModel();

  *instrument_icon_ = CreateInstrumentIcon(
      SK_ColorRED, kSecurePaymentConfirmationInstrumentIconDefaultWidthPx / 2,
      kSecurePaymentConfirmationInstrumentIconHeightPx / 2);

  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  views::ImageView* image_view = static_cast<views::ImageView*>(
      test_delegate_->dialog_view()->GetViewByID(static_cast<int>(
          SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_ICON)));
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconDefaultWidthPx,
            image_view->GetImageBounds().width());
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconHeightPx,
            image_view->GetImageBounds().height());

  CloseDialogAndWait();
}

// Test that a midsized instrument icon is not resized.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       MidsizedInstrumentIcon) {
  CreateModel();

  int width = (kSecurePaymentConfirmationInstrumentIconDefaultWidthPx +
               kSecurePaymentConfirmationInstrumentIconMaximumWidthPx) /
              2;
  *instrument_icon_ = CreateInstrumentIcon(
      SK_ColorRED, width, kSecurePaymentConfirmationInstrumentIconHeightPx);

  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  views::ImageView* image_view = static_cast<views::ImageView*>(
      test_delegate_->dialog_view()->GetViewByID(static_cast<int>(
          SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_ICON)));
  EXPECT_EQ(width, image_view->GetImageBounds().width());
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconHeightPx,
            image_view->GetImageBounds().height());

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       OptOutShownWhenRequested) {
  CreateModel();

  // Make sure that by default, opt-out wasn't requested. This means that every
  // other test is correctly testing the 'no opt out' path.
  ASSERT_FALSE(model_.opt_out_visible());

  // Now set it to true, and invoke SPC.
  model_.set_opt_out_visible(true);

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  ClickOptOutAndWait();
}

}  // namespace payments
