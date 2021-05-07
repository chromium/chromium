// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class VirtualCardManualFallbackBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest,
      public VirtualCardManualFallbackBubbleControllerImpl::ObserverForTest {
 public:
  // Various events that can be waited on by the DialogEventWaiter.
  enum class BubbleEvent : int {
    BUBBLE_SHOWN,
  };

  VirtualCardManualFallbackBubbleViewsInteractiveUiTest() = default;
  ~VirtualCardManualFallbackBubbleViewsInteractiveUiTest() override = default;
  VirtualCardManualFallbackBubbleViewsInteractiveUiTest(
      const VirtualCardManualFallbackBubbleViewsInteractiveUiTest&) = delete;
  VirtualCardManualFallbackBubbleViewsInteractiveUiTest& operator=(
      const VirtualCardManualFallbackBubbleViewsInteractiveUiTest&) = delete;

  // VirtualCardManualFallbackBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override {
    if (event_waiter_)
      event_waiter_->OnEvent(BubbleEvent::BUBBLE_SHOWN);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    VirtualCardManualFallbackBubbleControllerImpl* controller =
        static_cast<VirtualCardManualFallbackBubbleControllerImpl*>(
            VirtualCardManualFallbackBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    DCHECK(controller);
    controller->SetEventObserverForTesting(this);
  }

  void ShowBubble() {
    CreditCard card = test::GetFullServerCard();
    ResetEventWaiterForSequence({BubbleEvent::BUBBLE_SHOWN});
    GetController()->ShowBubble(&card,
                                /*virtual_card_cvc=*/u"123");
    event_waiter_->Wait();
  }

  VirtualCardManualFallbackBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  VirtualCardManualFallbackBubbleViews* GetBubbleViews() {
    VirtualCardManualFallbackBubbleControllerImpl* controller = GetController();
    if (!controller)
      return nullptr;

    return static_cast<VirtualCardManualFallbackBubbleViews*>(
        controller->GetBubble());
  }

  VirtualCardManualFallbackIconView* GetIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kVirtualCardManualFallback);
    DCHECK(icon);
    return static_cast<VirtualCardManualFallbackIconView*>(icon);
  }

  void ResetEventWaiterForSequence(std::list<BubbleEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<BubbleEvent>>(std::move(event_sequence));
  }

 private:
  std::unique_ptr<EventWaiter<BubbleEvent>> event_waiter_;
};

// Invokes a bubble showing the complete information for the virtual card
// selected to fill the form.
IN_PROC_BROWSER_TEST_F(VirtualCardManualFallbackBubbleViewsInteractiveUiTest,
                       ShowBubble) {
  ShowBubble();
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(GetIconView() && GetIconView()->GetVisible());
}

// Invokes the bubble and verifies the bubble is dismissed upon page navigation.
IN_PROC_BROWSER_TEST_F(VirtualCardManualFallbackBubbleViewsInteractiveUiTest,
                       DismissBubbleUponNavigation) {
  ShowBubble();
  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(GetIconView() && GetIconView()->GetVisible());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleViews()->GetWidget());
  ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com"));
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetBubbleViews());
  EXPECT_FALSE(GetIconView()->GetVisible());
}

}  // namespace autofill
