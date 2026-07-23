// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/payments/payment_app_loading_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace payments {

namespace {
constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests.";
}  // namespace

class PaymentAppLoadingViewInteractiveUiTest : public InteractiveBrowserTest {
 public:
  PaymentAppLoadingViewInteractiveUiTest() = default;
  PaymentAppLoadingViewInteractiveUiTest(
      const PaymentAppLoadingViewInteractiveUiTest&) = delete;
  PaymentAppLoadingViewInteractiveUiTest& operator=(
      const PaymentAppLoadingViewInteractiveUiTest&) = delete;
  ~PaymentAppLoadingViewInteractiveUiTest() override = default;

  InteractiveBrowserTestApi::MultiStep InvokeUiAndWaitForShow(
      ElementSpecifier element_specifier =
          views::DialogClientView::kTopViewId) {
    return Steps(
        Do([this]() {
          delegate_ = std::make_unique<views::DialogDelegate>();
          delegate_->SetOwnershipOfNewWidget(
              views::Widget::InitParams::CLIENT_OWNS_WIDGET);
          delegate_->SetModalType(ui::mojom::ModalType::kChild);
          delegate_->SetButtons(
              static_cast<int>(ui::mojom::DialogButton::kNone));
          auto loading_view = std::make_unique<PaymentAppLoadingView>(
              &icon_, GURL("https://app.com"), GURL("https://merchant.com"),
              close_callback_.Get());
          // The loading message label starts hidden and becomes visible only
          // after delay timer is fired. Since the dialog widget sizes itself to
          // the view's preferred size during initial creation, it would
          // initialize to a very small size (only housing the header) if we
          // didn't specify a larger size. When the label becomes visible
          // later, the widget window doesn't automatically resize, which would
          // cause the message to be clipped in the screenshot.
          loading_view->SetPreferredSize(
              gfx::Size(kPreferredPaymentHandlerDialogWidth,
                        kPreferredPaymentHandlerDialogHeight));
          loading_view_ = delegate_->SetContentsView(std::move(loading_view));

          tabs::TabInterface* tab_interface =
              tabs::TabInterface::GetFromContents(web_contents());
          widget_ = tab_interface->GetTabFeatures()
                        ->tab_dialog_manager()
                        ->CreateAndShowDialog(
                            delegate_.get(),
                            std::make_unique<tabs::TabDialogManager::Params>());
        }),
        InAnyContext(WaitForShow(element_specifier)));
  }

  void TearDownOnMainThread() override {
    loading_view_ = nullptr;
    widget_.reset();
    delegate_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  SkBitmap icon_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<views::DialogDelegate> delegate_;
  raw_ptr<PaymentAppLoadingView> loading_view_ = nullptr;
  base::MockRepeatingCallback<void(const ui::Event&)> close_callback_;
};

IN_PROC_BROWSER_TEST_F(PaymentAppLoadingViewInteractiveUiTest, InvokeUi) {
  RunTestSequence(
      InvokeUiAndWaitForShow(PaymentAppLoadingView::kTopViewId), Do([this]() {
        auto& timer = loading_view_->delay_message_timer_for_testing();
        if (timer.IsRunning()) {
          timer.FireNow();
        }
      }),
      InAnyContext(
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(PaymentAppLoadingView::kTopViewId,
                     /*screenshot_name=*/"payment_app_loading_view",
                     /*baseline_cl=*/"8100402")));
}

IN_PROC_BROWSER_TEST_F(PaymentAppLoadingViewInteractiveUiTest,
                       CloseButtonClicked) {
  EXPECT_CALL(close_callback_, Run);
  RunTestSequence(
      InvokeUiAndWaitForShow(),
      InAnyContext(PressButton(PaymentAppLoadingView::kCloseButtonId)));
}

}  // namespace payments
