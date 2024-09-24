// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/test/widget_test.h"

class HelpBubbleFactoryViewsUiTest : public InteractiveBrowserTest {
 public:
 protected:
  auto CreateHelpBubble(ui::ElementIdentifier anchor,
                        user_education::HelpBubbleParams params) {
    return Steps(
        WithElement(anchor,
                    [this, params = std::move(params)](
                        ui::TrackedElement* anchor) mutable {
                      BrowserView* const browser_view =
                          BrowserView::GetBrowserViewForBrowser(browser());
                      browser_native_view_ =
                          browser_view->GetWidget()->GetNativeView();
                      auto& registry =
                          UserEducationServiceFactory::GetForBrowserContext(
                              browser()->profile())
                              ->help_bubble_factory_registry();
                      help_bubble_ =
                          registry.CreateHelpBubble(anchor, std::move(params));
                    }),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                 [this](views::View* view) {
                   help_bubble_native_view_ =
                       view->GetWidget()->GetNativeView();
                 }));
  }

  auto CloseHelpBubble() {
    return Steps(
        Do([this]() { help_bubble_.reset(); }),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }

  static user_education::HelpBubbleParams GetDefaultHelpBubbleParams() {
    user_education::HelpBubbleParams params;
    params.body_text = u"Hello world!";
    return params;
  }

 protected:
  std::unique_ptr<user_education::HelpBubble> help_bubble_;
  gfx::NativeView browser_native_view_ = gfx::NativeView();
  gfx::NativeView help_bubble_native_view_ = gfx::NativeView();
};

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsUiTest,
                       ToggleFocusForAccessibility) {
  RunTestSequence(ObserveState(views::test::kCurrentWidgetFocus),
                  // A help bubble without buttons should not start focused.
                  CreateHelpBubble(kToolbarAppMenuButtonElementId,
                                   GetDefaultHelpBubbleParams()),
                  // Toggle focus to the help bubble.
                  Do([this]() { help_bubble_->ToggleFocusForAccessibility(); }),
                  WaitForState(views::test::kCurrentWidgetFocus,
                               std::ref(help_bubble_native_view_)),
                  // Toggle focus to the anchor view.
                  Do([this]() { help_bubble_->ToggleFocusForAccessibility(); }),
                  WaitForState(views::test::kCurrentWidgetFocus,
                               std::ref(browser_native_view_)),
                  CheckViewProperty(kToolbarAppMenuButtonElementId,
                                    &views::View::HasFocus, true),
                  CloseHelpBubble());
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsUiTest,
                       ToggleFocusForAccessibilityStartsFocused) {
  auto params = GetDefaultHelpBubbleParams();
  user_education::HelpBubbleButtonParams button_params;
  button_params.text = u"Button";
  button_params.is_default = true;
  params.buttons.emplace_back(std::move(button_params));

  RunTestSequence(
      ObserveState(views::test::kCurrentWidgetFocus),
      // A help bubble with a button should start focused.
      CreateHelpBubble(kToolbarAppMenuButtonElementId, std::move(params)),
      WaitForState(views::test::kCurrentWidgetFocus,
                   std::ref(help_bubble_native_view_)),
      // Toggle focus to the anchor view.
      Do([this]() { help_bubble_->ToggleFocusForAccessibility(); }),
      WaitForState(views::test::kCurrentWidgetFocus,
                   std::ref(browser_native_view_)),
      CheckViewProperty(kToolbarAppMenuButtonElementId, &views::View::HasFocus,
                        true),
      CloseHelpBubble());
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsUiTest,
                       ToggleFocusViaAccelerator) {
  ui::Accelerator next_pane;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_PANE, &next_pane));

  RunTestSequence(
      ObserveState(views::test::kCurrentWidgetFocus),
      CreateHelpBubble(kToolbarAppMenuButtonElementId,
                       GetDefaultHelpBubbleParams()),
      SendAccelerator(kBrowserViewElementId, next_pane),
      WaitForState(views::test::kCurrentWidgetFocus,
                   std::ref(help_bubble_native_view_)),
      SendAccelerator(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
          next_pane),
      WaitForState(views::test::kCurrentWidgetFocus,
                   std::ref(browser_native_view_)),
      CloseHelpBubble());
}
