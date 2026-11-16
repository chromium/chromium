// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace {

class TestSearchAIModeSignInPromoControllerObserver
    : public SearchAIModeSignInPromoController::Observer {
 public:
  void OnFlowAborted() override { flow_aborted_count_++; }

  int flow_aborted_count() const { return flow_aborted_count_; }

 private:
  int flow_aborted_count_ = 0;
};

// Disables `close_on_deactivate` when the target promo bubble initializes (so
// concurrent browser windows in parallel `browser_tests` cannot steal focus and
// close the bubble early) and records whether the widget was ever shown.
class PromoBubbleWidgetObserver {
 public:
  explicit PromoBubbleWidgetObserver(std::string widget_name)
      : widget_name_(std::move(widget_name)),
        observer_(views::test::AnyWidgetTestPasskey{}) {
    observer_.set_initialized_callback(
        base::BindLambdaForTesting([this](views::Widget* widget) {
          if (widget->GetName() == widget_name_) {
            if (auto* bubble =
                    widget->widget_delegate()->AsBubbleDialogDelegate()) {
              bubble->set_close_on_deactivate(false);
            }
          }
        }));
    observer_.set_shown_callback(
        base::BindLambdaForTesting([this](views::Widget* widget) {
          if (widget->GetName() == widget_name_) {
            widget_shown_ = true;
          }
        }));
  }

  bool widget_shown() const { return widget_shown_; }

 private:
  const std::string widget_name_;
  views::AnyWidgetObserver observer_;
  bool widget_shown_ = false;
};

class SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest
    : public InProcessBrowserTest {
 public:
  SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIToolbar,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         switches::kEnableSearchAIModeSigninPromo,
         contextual_tasks::kContextualTasks,
         omnibox::kComposeboxDriveContextMenuOptionSigninPromo},
        {});
  }

  bool WaitForAvatarElementVisible(bool expected_visible) {
    return base::test::RunUntil([&]() {
      return (BrowserElements::From(browser())->GetElement(
                  kToolbarAvatarButtonElementId) != nullptr) ==
             expected_visible;
    });
  }

  void OverflowWebUIToolbar() {
    ASSERT_TRUE(WaitForAvatarElementVisible(true));
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    WebUIToolbarWebView* webui_toolbar =
        browser_view->toolbar()->GetWebUIToolbarViewForTesting();
    ASSERT_TRUE(webui_toolbar);
    // Inject a non-shrinking spacer into `toolbar-app` to force responsive
    // toolbar overflow without hitting OS minimum browser window widths
    // (matching `WebUIToolbarWebViewTestBase::SetSpacerWidth`).
    const int spacer_width = browser_view->GetBounds().width();
    ASSERT_EQ(true, content::EvalJs(
                        webui_toolbar->GetWebViewForTesting()->GetWebContents(),
                        content::JsReplace(
                            R"((() => {
            const app = document.querySelector('toolbar-app');
            let spacer = app.shadowRoot.querySelector('#test-spacer');
            if (!spacer) {
              spacer = document.createElement('div');
              spacer.id = 'test-spacer';
              spacer.style.flexShrink = '0';
              app.shadowRoot.appendChild(spacer);
            }
            spacer.style.width = $1 + 'px';
            app.layoutResponsiveControls();
            return true;
          })();)",
                            spacer_width)));
    ASSERT_TRUE(WaitForAvatarElementVisible(false));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest,
    ControllerDestroyedWhilePending) {
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");

  base::RunLoop shown_loop;
  base::CallbackListSubscription shown_subscription =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kToolbarAvatarButtonElementId, browser_view->GetElementContext(),
          base::BindLambdaForTesting(
              [&](ui::TrackedElement*) { shown_loop.Quit(); }));

  auto destroyed_controller =
      std::make_unique<SearchAIModeSignInPromoController>(
          browser()->GetTabStripModel()->GetActiveWebContents());
  EXPECT_TRUE(destroyed_controller->MaybeShowPromo(browser_view));
  EXPECT_TRUE(avatar_button->HasExplicitButtonState());

  // Destroying the controller resets the pill text immediately and cancels the
  // bound callback; once the queued WebUI pop-out finishes, its held
  // `keep_visible` closure is dropped and the avatar button re-overflows.
  destroyed_controller.reset();
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
  shown_loop.Run();
  EXPECT_TRUE(WaitForAvatarElementVisible(false));
  EXPECT_FALSE(widget_observer.widget_shown());
  EXPECT_EQ(browser()->GetProfile()->GetPrefs()->GetInteger(
                prefs::kSearchAIModeSignInPromoShownCountPerProfile),
            0);
}

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest,
    TabClosedWhilePending) {
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");
  TestSearchAIModeSignInPromoControllerObserver observer;

  chrome::AddTabAt(browser(), GURL("about:blank"), /*index=*/-1,
                   /*foreground=*/true);
  content::WebContents* closing_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  SearchAIModeSignInPromoController controller(closing_tab);
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoController::Observer>
      observation(&observer);
  observation.Observe(&controller);

  EXPECT_TRUE(controller.MaybeShowPromo(browser_view));
  EXPECT_TRUE(avatar_button->HasExplicitButtonState());
  browser()->GetTabStripModel()->CloseWebContentsAt(
      browser()->GetTabStripModel()->active_index(),
      TabCloseTypes::CLOSE_USER_GESTURE);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.flow_aborted_count() == 1; }));
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
  EXPECT_TRUE(WaitForAvatarElementVisible(false));
  EXPECT_FALSE(widget_observer.widget_shown());
  EXPECT_EQ(browser()->GetProfile()->GetPrefs()->GetInteger(
                prefs::kSearchAIModeSignInPromoShownCountPerProfile),
            0);
}

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest,
    TabHiddenWhilePending) {
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");
  TestSearchAIModeSignInPromoControllerObserver observer;

  SearchAIModeSignInPromoController controller(
      browser()->GetTabStripModel()->GetActiveWebContents());
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoController::Observer>
      observation(&observer);
  observation.Observe(&controller);

  EXPECT_TRUE(controller.MaybeShowPromo(browser_view));
  EXPECT_TRUE(avatar_button->HasExplicitButtonState());

  // Switching to a new foreground tab hides the promo's tab synchronously
  // before `RunWhenVisible()` resolves.
  chrome::AddTabAt(browser(), GURL("about:blank"), /*index=*/-1,
                   /*foreground=*/true);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.flow_aborted_count() == 1; }));
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
  EXPECT_TRUE(WaitForAvatarElementVisible(false));
  EXPECT_FALSE(widget_observer.widget_shown());
  EXPECT_EQ(browser()->GetProfile()->GetPrefs()->GetInteger(
                prefs::kSearchAIModeSignInPromoShownCountPerProfile),
            0);
}

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest,
    ProfileIneligibleWhilePending) {
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");
  TestSearchAIModeSignInPromoControllerObserver observer;

  SearchAIModeSignInPromoController controller(
      browser()->GetTabStripModel()->GetActiveWebContents());
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoController::Observer>
      observation(&observer);
  observation.Observe(&controller);

  EXPECT_TRUE(controller.MaybeShowPromo(browser_view));
  EXPECT_TRUE(avatar_button->HasExplicitButtonState());

  // Reaching the impression cap before `RunWhenVisible()` resolves makes the
  // profile ineligible when `ShowPromoWithAnchor()` re-checks `CanShowPromo()`.
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kSearchAIModeSignInPromoShownCountPerProfile, 5);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.flow_aborted_count() == 1; }));
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
  EXPECT_TRUE(WaitForAvatarElementVisible(false));
  EXPECT_FALSE(widget_observer.widget_shown());
}

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest,
    TabMovedToAnotherWindowWhilePending) {
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  chrome::AddTabAt(browser(), GURL("about:blank"), /*index=*/-1,
                   /*foreground=*/true);
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");
  TestSearchAIModeSignInPromoControllerObserver observer;

  SearchAIModeSignInPromoController controller(
      browser()->GetTabStripModel()->GetActiveWebContents());
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoController::Observer>
      observation(&observer);
  observation.Observe(&controller);

  EXPECT_TRUE(controller.MaybeShowPromo(browser_view));
  other_browser->GetTabStripModel()->AppendTab(
      browser()->GetTabStripModel()->DetachTabAtForInsertion(
          browser()->GetTabStripModel()->active_index()),
      /*foreground=*/true);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.flow_aborted_count() == 1; }));
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
  EXPECT_TRUE(WaitForAvatarElementVisible(false));
  EXPECT_FALSE(widget_observer.widget_shown());
}

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest,
    TabNavigatedWhilePending) {
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  WebUIToolbarWebView* webui_toolbar =
      browser_view->toolbar()->GetWebUIToolbarViewForTesting();
  ASSERT_TRUE(webui_toolbar);
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");
  TestSearchAIModeSignInPromoControllerObserver observer;

  // Hold the inner WebUI avatar button hidden while `NavigateToURL()` spins a
  // `RunLoop`, so `RunWhenVisible()` resolves deterministically after the
  // cross-document navigation commits.
  ASSERT_EQ(true, content::EvalJs(
                      webui_toolbar->GetWebViewForTesting()->GetWebContents(),
                      R"((() => {
          document.querySelector('toolbar-app')
              .shadowRoot.querySelector('#avatar')
              .shadowRoot.querySelector('#button').style.display = 'none';
          return true;
        })();)"));

  SearchAIModeSignInPromoController controller(
      browser()->GetTabStripModel()->GetActiveWebContents());
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoController::Observer>
      observation(&observer);
  observation.Observe(&controller);

  EXPECT_TRUE(controller.MaybeShowPromo(browser_view));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));

  ASSERT_EQ(true, content::EvalJs(
                      webui_toolbar->GetWebViewForTesting()->GetWebContents(),
                      R"((() => {
          document.querySelector('toolbar-app')
              .shadowRoot.querySelector('#avatar')
              .shadowRoot.querySelector('#button').style.display = '';
          return true;
        })();)"));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.flow_aborted_count() == 1; }));
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
  EXPECT_TRUE(WaitForAvatarElementVisible(false));
  EXPECT_FALSE(widget_observer.widget_shown());
  EXPECT_EQ(browser()->GetProfile()->GetPrefs()->GetInteger(
                prefs::kSearchAIModeSignInPromoShownCountPerProfile),
            0);
}

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest,
    ShowsBubbleAndReoverflowsOnClose) {
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SearchAIModeSignInPromoView");
  TestSearchAIModeSignInPromoControllerObserver observer;

  SearchAIModeSignInPromoController controller(
      browser()->GetTabStripModel()->GetActiveWebContents());
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoController::Observer>
      observation(&observer);
  observation.Observe(&controller);

  EXPECT_TRUE(controller.MaybeShowPromo(browser_view));

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(WaitForAvatarElementVisible(true));
  EXPECT_TRUE(avatar_button->HasExplicitButtonState());
  EXPECT_EQ(observer.flow_aborted_count(), 0);

  auto* bubble_delegate = widget->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_delegate);
  ui::TrackedElement* avatar_element =
      BrowserElements::From(browser())->GetElement(
          kToolbarAvatarButtonElementId);
  ASSERT_TRUE(avatar_element);
  EXPECT_TRUE(
      bubble_delegate->IsSameAnchor(views::BubbleAnchor(avatar_element)));

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter.Wait();
  EXPECT_EQ(observer.flow_aborted_count(), 1);
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
  EXPECT_TRUE(WaitForAvatarElementVisible(false));
}

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerWebUIAvatarOverflowBrowserTest,
    ComposeboxDriveBubbleStaysVisibleAfterControllerResetUntilClosed) {
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar());

  PromoBubbleWidgetObserver widget_observer("ComposeboxDriveSignInPromoView");
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "ComposeboxDriveSignInPromoView");

  auto controller = std::make_unique<ComposeboxDriveSignInPromoController>(
      browser()->GetTabStripModel()->GetActiveWebContents());
  EXPECT_TRUE(controller->MaybeShowPromo(browser()));

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(WaitForAvatarElementVisible(true));

  auto* bubble_delegate = widget->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_delegate);
  ui::TrackedElement* avatar_element =
      BrowserElements::From(browser())->GetElement(
          kToolbarAvatarButtonElementId);
  ASSERT_TRUE(avatar_element);
  EXPECT_TRUE(
      bubble_delegate->IsSameAnchor(views::BubbleAnchor(avatar_element)));

  // Resetting the controller while the Drive promo bubble is still open (as
  // `ContextualSearchboxHandler::OnActiveTabNavigated` does on title updates)
  // must not release `keep_visible_` before the bubble closes. Flush two
  // renderer animation frames on the WebUI toolbar to verify no overflow IPC
  // was dispatched by `controller.reset()`.
  controller.reset();
  WebUIToolbarWebView* webui_toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->GetWebUIToolbarViewForTesting();
  ASSERT_TRUE(webui_toolbar);
  ASSERT_EQ(true, content::EvalJs(
                      webui_toolbar->GetWebViewForTesting()->GetWebContents(),
                      "new Promise(r => requestAnimationFrame(() => "
                      "requestAnimationFrame(() => r(true))))"));
  EXPECT_NE(BrowserElements::From(browser())->GetElement(
                kToolbarAvatarButtonElementId),
            nullptr);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter.Wait();
  EXPECT_TRUE(WaitForAvatarElementVisible(false));
}

class SearchAIModeSignInPromoControllerViewsOverflowBrowserTest
    : public InProcessBrowserTest {
 public:
  SearchAIModeSignInPromoControllerViewsOverflowBrowserTest() {
    ToolbarControllerUtil::SetPreventOverflowForTesting(false);
    feature_list_.InitWithFeatures(
        {switches::kEnableSearchAIModeSigninPromo,
         contextual_tasks::kContextualTasks},
        {features::kInitialWebUI, features::kWebUIAvatarButton,
         features::kWebUIToolbar});
  }

  ~SearchAIModeSignInPromoControllerViewsOverflowBrowserTest() override {
    ToolbarControllerUtil::SetPreventOverflowForTesting(true);
  }

  void OverflowViewsToolbar(BrowserView* browser_view) {
    ToolbarView* toolbar = browser_view->toolbar();
    constexpr gfx::Size kDummySize(34, 34);
    int added_width = 0;
    while (added_width <= toolbar->width()) {
      auto dummy = std::make_unique<ToolbarButton>();
      dummy->SetPreferredSize(kDummySize);
      dummy->SetMinSize(kDummySize);
      dummy->GetViewAccessibility().SetName(u"dummy");
      dummy->SetVisible(true);
      toolbar->AddChildView(std::move(dummy));
      added_width += kDummySize.width();
      views::test::RunScheduledLayout(browser_view);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerViewsOverflowBrowserTest,
    BackgroundTabAbortsSynchronouslyAndReturnsFalse) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");
  TestSearchAIModeSignInPromoControllerObserver observer;

  chrome::AddTabAt(browser(), GURL("about:blank"), /*index=*/-1,
                   /*foreground=*/false);
  content::WebContents* background_tab =
      browser()->GetTabStripModel()->GetWebContentsAt(1);
  ASSERT_EQ(background_tab->GetVisibility(), content::Visibility::HIDDEN);

  SearchAIModeSignInPromoController controller(background_tab);
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoController::Observer>
      observation(&observer);
  observation.Observe(&controller);

  EXPECT_FALSE(controller.MaybeShowPromo(browser_view));
  EXPECT_EQ(observer.flow_aborted_count(), 1);
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
  EXPECT_FALSE(widget_observer.widget_shown());
}

IN_PROC_BROWSER_TEST_F(
    SearchAIModeSignInPromoControllerViewsOverflowBrowserTest,
    OverflowedViewsAvatarPopsOutUntilBubbleClosed) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButton* views_avatar_button =
      browser_view->toolbar()->avatar_toolbar_button();
  ASSERT_TRUE(views_avatar_button);

  OverflowViewsToolbar(browser_view);
  ASSERT_FALSE(views_avatar_button->GetVisible());

  PromoBubbleWidgetObserver widget_observer("SearchAIModeSignInPromoView");
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SearchAIModeSignInPromoView");
  TestSearchAIModeSignInPromoControllerObserver observer;

  SearchAIModeSignInPromoController controller(
      browser()->GetTabStripModel()->GetActiveWebContents());
  base::ScopedObservation<SearchAIModeSignInPromoController,
                          SearchAIModeSignInPromoController::Observer>
      observation(&observer);
  observation.Observe(&controller);

  EXPECT_TRUE(controller.MaybeShowPromo(browser_view));
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(views_avatar_button->GetVisible());
  EXPECT_TRUE(views_avatar_button->HasExplicitButtonState());

  auto* bubble_delegate = widget->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_delegate);
  EXPECT_EQ(bubble_delegate->GetAnchorView(), views_avatar_button);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter.Wait();
  views::test::RunScheduledLayout(browser_view);

  EXPECT_FALSE(views_avatar_button->GetVisible());
  EXPECT_FALSE(views_avatar_button->HasExplicitButtonState());
  EXPECT_EQ(observer.flow_aborted_count(), 1);
}

}  // namespace
