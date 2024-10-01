// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);

class OmniboxInputWaiter : public OmniboxTabHelper::Observer {
 public:
  explicit OmniboxInputWaiter(content::WebContents* web_contents) {
    omnibox_helper_observer_.Observe(
        OmniboxTabHelper::FromWebContents(web_contents));

    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
  }
  ~OmniboxInputWaiter() override = default;

  void Wait() { run_loop_->Run(); }

  void OnOmniboxInputStateChanged() override {}

  void OnOmniboxInputInProgress(bool in_progress) override {
    run_loop_->Quit();
  }

  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override {}

  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override {}

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      omnibox_helper_observer_{this};
};
}  // namespace

class ToastControllerInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {toast_features::kToastFramework, toast_features::kLinkCopiedToast,
         toast_features::kImageCopiedToast, toast_features::kReadingListToast},
        {});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetURL(std::string_view hostname = "example.com",
              std::string_view path = "/title1.html") {
    return embedded_test_server()->GetURL(hostname, path);
  }

  ToastController* GetToastController() {
    return browser()->browser_window_features()->toast_controller();
  }

  auto ShowToast(ToastParams params) {
    return Do(base::BindOnce(
        [](ToastController* toast_controller, ToastParams toast_params) {
          toast_controller->MaybeShowToast(std::move(toast_params));
        },
        GetToastController(), std::move(params)));
  }

  auto FireToastCloseTimer() {
    return Do([=, this]() {
      GetToastController()->GetToastCloseTimerForTesting()->FireNow();
    });
  }

  auto CheckShowingToastId(ToastId expected_id) {
    return CheckResult(
        [=, this]() {
          ToastController* const toast_controller = GetToastController();
          std::optional<ToastId> current_toast_id =
              toast_controller->GetCurrentToastId();
          return current_toast_id.value();
        },
        expected_id);
  }

  auto AdvanceKeyboardFocus(bool reverse) {
    return Do([this, reverse]() {
      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
          browser(), ui::VKEY_TAB, false, reverse, false, false));
    });
  }

  void RemoveOmniboxFocus() {
    ui_test_utils::ClickOnView(
        BrowserView::GetBrowserViewForBrowser(browser())->contents_web_view());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, ShowEphemeralToast) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       ShowSameEphemeralToastTwice) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }),
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, PreemptEphemeralToast) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }),
      ShowToast(ToastParams(ToastId::kImageCopied)));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, ShowPersistentToast) {
  RunTestSequence(ShowToast(ToastParams(ToastId::kLensOverlay)),
                  WaitForShow(toasts::ToastView::kToastViewId), Check([=, this]() {
                    return GetToastController()->IsShowingToast();
                  }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, PersistentToastHides) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLensOverlay)),
      WaitForShow(toasts::ToastView::kToastViewId), Do([=, this]() {
        GetToastController()->ClosePersistentToast(ToastId::kLensOverlay);
      }),
      WaitForHide(toasts::ToastView::kToastViewId));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, PreemptPersistentToast) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLensOverlay)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }),
      CheckShowingToastId(ToastId::kLensOverlay),
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      // Ephemeral Toast should force the persistent toast to close
      WaitForHide(toasts::ToastView::kToastViewId),
      // After the persistent toast closes, the ephemeral toast should show
      WaitForShow(toasts::ToastView::kToastViewId),
      CheckShowingToastId(ToastId::kLinkCopied),
      // Simulate the ephemeral toast timing out and auto dismiss
      FireToastCloseTimer(), WaitForHide(toasts::ToastView::kToastViewId),
      // Persistent toast should reshow
      WaitForShow(toasts::ToastView::kToastViewId),
      CheckShowingToastId(ToastId::kLensOverlay));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, FocusNextPane) {
  ui::Accelerator next_pane;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_PANE, &next_pane));
  views::Widget* toast_widget = nullptr;
  RunTestSequence(
      ObserveState(views::test::kCurrentWidgetFocus),
      ShowToast(ToastParams(ToastId::kAddedToReadingList)),
      WaitForShow(toasts::ToastView::kToastViewId),
      WithView(
          toasts::ToastView::kToastViewId,
          [&](toasts::ToastView* toast) { toast_widget = toast->GetWidget(); }),
      CheckView(toasts::ToastView::kToastViewId,
                [](toasts::ToastView* toast) {
                  return !toast->GetFocusManager()->GetFocusedView();
                }),
      SendAccelerator(kBrowserViewElementId, next_pane),
      WaitForState(views::test::kCurrentWidgetFocus,
                   [&]() { return toast_widget->GetNativeView(); }),
      CheckView(toasts::ToastView::kToastViewId, [](toasts::ToastView* toast) {
        return toast->GetFocusManager()->GetFocusedView() ==
               toast->action_button_for_testing();
      }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, ReverseFocusTraversal) {
  ui::Accelerator next_pane;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_PANE, &next_pane));
  RunTestSequence(
      ObserveState(views::test::kCurrentWidgetFocus),
      ShowToast(ToastParams(ToastId::kAddedToReadingList)),
      WaitForShow(toasts::ToastView::kToastViewId),
      ActivateSurface(toasts::ToastView::kToastViewId),
      SendAccelerator(kBrowserViewElementId, next_pane),
      CheckView(toasts::ToastView::kToastViewId,
                [](toasts::ToastView* toast) {
                  return toast->GetFocusManager()->GetFocusedView() ==
                         toast->action_button_for_testing();
                }),
      AdvanceKeyboardFocus(true),
#if BUILDFLAG(IS_MAC)
      // Mac focus traversal order is slightly different from other platforms
      CheckView(kToolbarAppMenuButtonElementId,
                [](AppMenuButton* button) { return button->HasFocus(); })
#else
        CheckView(kBookmarkStarViewElementId,
                [](StarView* star_view) { return star_view->HasFocus(); })
#endif
  );
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, ForwardFocusTraversal) {
  ui::Accelerator next_pane;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_PANE, &next_pane));
  RunTestSequence(
      ObserveState(views::test::kCurrentWidgetFocus),
      ShowToast(ToastParams(ToastId::kAddedToReadingList)),
      WaitForShow(toasts::ToastView::kToastViewId),
      ActivateSurface(toasts::ToastView::kToastViewId),
      SendAccelerator(kBrowserViewElementId, next_pane),
      // Advancing focus should move into the toast close button
      AdvanceKeyboardFocus(false),
      CheckView(toasts::ToastView::kToastViewId,
                [](toasts::ToastView* toast) {
                  return toast->close_button_for_testing()->HasFocus();
                }),
      // Advancing focus again should move out of the toast and into the WebView
      AdvanceKeyboardFocus(false),
      CheckView(toasts::ToastView::kToastViewId,
                [](toasts::ToastView* toast) {
                  return !toast->close_button_for_testing()->HasFocus();
                }),
      CheckView(kBrowserViewElementId, [](BrowserView* browser_view) {
        return browser_view->GetContentsWebView()->HasFocus();
      }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       HideTabScopedToastOnTabChange) {
  RunTestSequence(InstrumentTab(kFirstTab),
                  AddInstrumentedTab(kSecondTab, GetURL()),
                  SelectTab(kTabStripElementId, 0), WaitForShow(kFirstTab),
                  ShowToast(ToastParams(ToastId::kLinkCopied)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  SelectTab(kTabStripElementId, 1),
                  WaitForHide(toasts::ToastView::kToastViewId));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       GlobalScopedToastStaysOnTabChange) {
  RunTestSequence(InstrumentTab(kFirstTab),
                  AddInstrumentedTab(kSecondTab, GetURL()),
                  SelectTab(kTabStripElementId, 0), WaitForShow(kFirstTab),
                  ShowToast(ToastParams(ToastId::kNonMilestoneUpdate)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  SelectTab(kTabStripElementId, 1),
                  EnsurePresent(toasts::ToastView::kToastViewId));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       HideTabScopedToastOnNavigation) {
  RunTestSequence(InstrumentTab(kFirstTab),
                  ShowToast(ToastParams(ToastId::kLinkCopied)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  NavigateWebContents(kFirstTab, GetURL()),
                  WaitForHide(toasts::ToastView::kToastViewId));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       GlobalScopedToastStaysOnNavigation) {
  RunTestSequence(InstrumentTab(kFirstTab),
                  ShowToast(ToastParams(ToastId::kNonMilestoneUpdate)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  NavigateWebContents(kFirstTab, GetURL()),
                  EnsurePresent(toasts::ToastView::kToastViewId));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       ToastReactToOmniboxFocus) {
  LocationBar* const location_bar = browser()->window()->GetLocationBar();
  ASSERT_TRUE(location_bar);
  OmniboxView* const omnibox_view = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);
  browser()->window()->SetFocusToLocationBar(true);
  ASSERT_FALSE(omnibox_view->model()->PopupIsOpen());

  // Even though the omnibox is focused, the toast should still show because
  // the omnibox doesn't have a popup and the user isn't interacting with the
  // omnibox.
  ToastController* const toast_controller = GetToastController();
  EXPECT_TRUE(
      toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_TRUE(toast_controller->GetToastWidgetForTesting()->IsVisible());

  // Omnibox should still show even when focus is removed from the omnibox.
  RemoveOmniboxFocus();
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_TRUE(toast_controller->GetToastWidgetForTesting()->IsVisible());

  // Focus the omnibox again should cause the toast to no longer be visible
  // because we are focusing after the toast is already shown.
  browser()->window()->SetFocusToLocationBar(true);
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_FALSE(toast_controller->GetToastWidgetForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       HidesWhenOmniboxPopupShows) {
  // Even though the omnibox is focused, the toast should still show because
  // the omnibox doesn't have a popup and the user isn't interacting with the
  // omnibox.
  ToastController* const toast_controller = GetToastController();
  EXPECT_TRUE(
      toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_TRUE(toast_controller->GetToastWidgetForTesting()->IsVisible());

  // Trigger the omnibox popup to show.
  LocationBar* const location_bar = browser()->window()->GetLocationBar();
  ASSERT_TRUE(location_bar);
  OmniboxView* const omnibox_view = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);
  ASSERT_FALSE(omnibox_view->model()->PopupIsOpen());
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"hello world");
  omnibox_view->OnAfterPossibleChange(true);

  ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

  // The toast widget should no longer be visible because there is a popup.
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_FALSE(toast_controller->GetToastWidgetForTesting()->IsVisible());

  // Toast widget is visible again after the omnibox is no longer focused.
  RemoveOmniboxFocus();
  ASSERT_FALSE(omnibox_view->model()->PopupIsOpen());
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_TRUE(toast_controller->GetToastWidgetForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       HidesWhenTypingInOmnibox) {
  browser()->window()->SetFocusToLocationBar(true);

  // Even though the omnibox is focused, the toast should still show because
  // the omnibox doesn't have a popup and the user isn't interacting with the
  // omnibox.
  ToastController* const toast_controller = GetToastController();
  EXPECT_TRUE(
      toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_TRUE(toast_controller->GetToastWidgetForTesting()->IsVisible());

  // Start typing in the omnibox.
  auto omnibox_input_waiter = std::make_unique<OmniboxInputWaiter>(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  omnibox_input_waiter->Wait();

  // The toast widget should no longer be visible because we are typing.
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_FALSE(toast_controller->GetToastWidgetForTesting()->IsVisible());

  // Toast widget is visible again after the omnibox is no longer focused.
  RemoveOmniboxFocus();
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_TRUE(toast_controller->GetToastWidgetForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, RecordToastShows) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetURL()),
      SelectTab(kTabStripElementId, 0), WaitForShow(kFirstTab), Do([&]() {
        histogram_tester.ExpectBucketCount("Toast.TriggeredToShow",
                                           ToastId::kLinkCopied, 0);
      }),
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId), Do([&]() {
        histogram_tester.ExpectBucketCount("Toast.TriggeredToShow",
                                           ToastId::kLinkCopied, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       RecordToastDismissReason) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetURL()),
      SelectTab(kTabStripElementId, 0), WaitForShow(kFirstTab), Do([&]() {
        histogram_tester.ExpectBucketCount("Toast.LinkCopied.Dismissed",
                                           toasts::ToastCloseReason::kAbort, 0);
      }),
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      SelectTab(kTabStripElementId, 1),
      WaitForHide(toasts::ToastView::kToastViewId), Do([&]() {
        histogram_tester.ExpectBucketCount("Toast.LinkCopied.Dismissed",
                                           toasts::ToastCloseReason::kAbort, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       ToastRendersOverWebContents) {
#if BUILDFLAG(IS_MAC)
  FullscreenController* const fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  fullscreen_controller->set_is_tab_fullscreen_for_testing(true);
#else
  ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
  content::WebContents* const active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  active_contents->GetDelegate()->EnterFullscreenModeForTab(
      active_contents->GetPrimaryMainFrame(), {});
  waiter.Wait();
#endif

  ToastController* const toast_controller = GetToastController();
  EXPECT_TRUE(
      toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  const gfx::Rect toast_bounds =
      toast_controller->GetToastViewForTesting()->GetBoundsInScreen();
  const gfx::Rect web_view_bounds =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetContentsWebView()
          ->GetBoundsInScreen();
  EXPECT_TRUE(web_view_bounds.Contains(toast_bounds));
}
