// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_opt_in_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class MandatoryReauthBubbleViewUiTest : public InProcessBrowserTest {
 public:
  MandatoryReauthBubbleViewUiTest() = default;
  ~MandatoryReauthBubbleViewUiTest() override = default;
  MandatoryReauthBubbleViewUiTest(const MandatoryReauthBubbleViewUiTest&) =
      delete;
  MandatoryReauthBubbleViewUiTest& operator=(
      const MandatoryReauthBubbleViewUiTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    MandatoryReauthBubbleControllerImpl::CreateForWebContents(web_contents);
    MandatoryReauthBubbleControllerImpl* controller = GetController();
    DCHECK(controller);
  }

  void ShowBubble() {
    MandatoryReauthBubbleControllerImpl* controller = GetController();
    controller->ShowBubble(base::DoNothing(), base::DoNothing(),
                           base::DoNothing());
    views::test::WidgetVisibleWaiter visible_waiter(
        static_cast<MandatoryReauthOptInBubbleView*>(
            controller->GetBubbleView())
            ->GetWidget());
    visible_waiter.Wait();
  }

  bool IsIconVisible() { return GetIconView() && GetIconView()->GetVisible(); }

  MandatoryReauthBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return MandatoryReauthBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  views::BubbleDialogDelegate* GetOptInBubble() {
    return GetIconView()->GetBubble();
  }

  MandatoryReauthIconView* GetIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kMandatoryReauth);
    DCHECK(icon);
    return static_cast<MandatoryReauthIconView*>(icon);
  }

  void ClickOnView(views::View* view) {
    ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(pressed);
    ui::MouseEvent released_event =
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(released_event);
  }

  void ClickOnViewAndWait(
      views::View* view,
      views::BubbleDialogDelegate* mandatory_reauth_bubble) {
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        mandatory_reauth_bubble->GetWidget());
    mandatory_reauth_bubble->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(
            mandatory_reauth_bubble->GetWidget()
                ->non_client_view()
                ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
    destroyed_waiter.Wait();
  }

  void ClickOnOkButton(views::BubbleDialogDelegate* mandatory_reauth_bubble) {
    views::View* ok_button = mandatory_reauth_bubble->GetOkButton();
    ClickOnViewAndWait(ok_button, mandatory_reauth_bubble);
  }

  void ClickOnCancelButton(
      views::BubbleDialogDelegate* mandatory_reauth_bubble) {
    views::View* cancel_button = mandatory_reauth_bubble->GetCancelButton();
    ClickOnViewAndWait(cancel_button, mandatory_reauth_bubble);
  }

 protected:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
};

IN_PROC_BROWSER_TEST_F(MandatoryReauthBubbleViewUiTest, ShowBubble) {
  ShowBubble();
  EXPECT_TRUE(GetOptInBubble());
  EXPECT_TRUE(IsIconVisible());
}

IN_PROC_BROWSER_TEST_F(MandatoryReauthBubbleViewUiTest,
                       ClickOptInCancelButton) {
  ShowBubble();
  ClickOnCancelButton(GetOptInBubble());
  EXPECT_FALSE(GetOptInBubble());
  EXPECT_FALSE(IsIconVisible());
}

IN_PROC_BROWSER_TEST_F(MandatoryReauthBubbleViewUiTest, ClickOptInOkButton) {
  ShowBubble();
  ClickOnOkButton(GetOptInBubble());
  EXPECT_FALSE(GetOptInBubble());
  EXPECT_FALSE(IsIconVisible());
}

}  // namespace autofill
