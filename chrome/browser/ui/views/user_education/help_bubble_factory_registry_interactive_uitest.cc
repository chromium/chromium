// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

class HelpBubbleFactoryRegistryInteractiveUitest
    : public InteractiveBrowserTest {
 public:
  HelpBubbleFactoryRegistryInteractiveUitest() = default;
  ~HelpBubbleFactoryRegistryInteractiveUitest() override = default;

 protected:
  user_education::HelpBubbleParams GetBubbleParams() {
    user_education::HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = user_education::HelpBubbleArrow::kRightTop;
    return params;
  }

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  user_education::HelpBubbleFactoryRegistry* GetRegistry() {
    return &UserEducationServiceFactory::GetForBrowserContext(
                browser()->profile())
                ->help_bubble_factory_registry();
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryRegistryInteractiveUitest,
                       AnchorHelpBubbleToViewsMenuItem) {
  std::unique_ptr<user_education::HelpBubble> bubble;

  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      // Show and verify the bubble.
      WithElement(AppMenuModel::kHistoryMenuItem,
                  [this, &bubble](ui::TrackedElement* item) {
                    bubble = GetRegistry()->CreateHelpBubble(item,
                                                             GetBubbleParams());
                  }),
      Check([&bubble]() { return bubble != nullptr; },
            "Check bubble is not null."),
      Check([&bubble]() { return bubble->is_open(); },
            "Check bubble registers as open."),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Ensure that focus is not taken by the bubble.
      Check([this]() { return GetBrowserView()->GetWidget()->IsActive(); }));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryRegistryInteractiveUitest,
                       AnchorHelpBubbleToContextMenuItem) {
  std::unique_ptr<user_education::HelpBubble> bubble;

  RunTestSequence(
      // To detect race conditions, ensure that nothing is waiting to execute in
      // the background before the test starts. See discussion on
      // https://crbug.com/347282481 for why clearing the message queue can
      // unearth specific bugs.

      // Trigger the context menu.
      Do([this]() {
        // Have to defer opening because this call is blocking on Mac;
        // subsequent steps will be called from within the run loop of the
        // context menu.
        //
        // On Mac, this works like:
        //  Test Loop -> Post Task -> Register for menu item shown callback
        //            -> Show context menu -> Callback called ->
        //                                      Bubble Shown, Bubble verified,
        //                                      Context menu closed
        //
        // On other platforms, this kicks off a normal event sequence where we
        // wait for each element to appear and then do the next step.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindLambdaForTesting([this]() {
              auto* tab = GetBrowserView()->tabstrip()->tab_at(0);
              tab->ShowContextMenu(tab->bounds().CenterPoint(),
                                   ui::MenuSourceType::MENU_SOURCE_MOUSE);
            }));
      }),

#if BUILDFLAG(IS_MAC)
      // Because context menus run inside of a system message pump that cannot
      // process Chrome tasks, the following steps must be executed immediately
      // on the platform.
      WithoutDelay(Steps(
#endif
          // This step should still trigger even inside the Mac context menu
          // loop because it runs immediately on the callback from the menu item
          // being created (see ElementTrackerMac for code paths).
          WaitForShow(TabMenuModel::kAddToNewGroupItemIdentifier),

          // Create and wait for the help bubble.
          WithElement(TabMenuModel::kAddToNewGroupItemIdentifier,
                      [this, &bubble](ui::TrackedElement* el) {
                        bubble = GetRegistry()->CreateHelpBubble(
                            el, GetBubbleParams());
                      }),
          WaitForShow(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),

          // For platforms where context menus run synchronously, if we don't
          // close the menu, we will get stuck in the inner message pump and can
          // never finish the test. On other platforms, this is harmless and
          // will clean up all of the secondary UI.
          Do([this]() {
            static_cast<BrowserTabStripController*>(
                GetBrowserView()->tabstrip()->controller())
                ->CloseContextMenuForTesting();
          })
#if BUILDFLAG(IS_MAC)
              ))  // WithoutDelay(Steps(
#endif
  );
}
