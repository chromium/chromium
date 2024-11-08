// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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
#include "components/plus_addresses/features.h"
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

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSampleMenuItem);

// Simplified menu model with a single item that runs `closure` on selecting it.
class TestMenuModel : public ui::SimpleMenuModel,
                      ui::SimpleMenuModel::Delegate {
 public:
  explicit TestMenuModel(base::RepeatingClosure closure)
      : ui::SimpleMenuModel(/*delegate=*/this), closure_(std::move(closure)) {
    AddItem(kCommandId, u"Some entry");
    SetElementIdentifierAt(0, kSampleMenuItem);
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    ASSERT_EQ(command_id, kCommandId);
    closure_.Run();
  }

 private:
  static constexpr int kCommandId = 123;

  base::RepeatingClosure closure_;
};

}  // namespace

class ToastControllerInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {toast_features::kToastFramework, toast_features::kLinkCopiedToast,
         toast_features::kImageCopiedToast, toast_features::kReadingListToast,
         plus_addresses::features::kPlusAddressesEnabled,
         plus_addresses::features::kPlusAddressFullFormFill},
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

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, ShowToast) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, ShowSameToastTwice) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }),
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, PreemptToast) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=, this]() { return GetToastController()->IsShowingToast(); }),
      ShowToast(ToastParams(ToastId::kImageCopied)));
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

// Tests that setting a menu model in `ToastParams` adds a menu button to the
// toast that runs the menu model and that interacting with a menu element
// closes the toast.
IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       MenuButtonClickOpensMenu) {
  ToastParams params(ToastId::kPlusAddressOverride);
  int counter = 0;
  params.menu_model = std::make_unique<TestMenuModel>(
      base::BindLambdaForTesting([&counter]() { ++counter; }));
  RunTestSequence(ShowToast(std::move(params)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  EnsurePresent(toasts::ToastView::kToastMenuButton),
                  PressButton(toasts::ToastView::kToastMenuButton),
                  WaitForShow(kSampleMenuItem), SelectMenuItem(kSampleMenuItem),
                  WaitForHide(toasts::ToastView::kToastViewId),
                  Check([&]() { return counter == 1; }));
}

// Tests that attempting to close the `ToastView` does not succeed while the
// menu is open. If that happens, the `ToastView` is closed once the menu
// closes.
IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       ToastDoesNotCloseWhileMenuIsOpen) {
  ToastParams params(ToastId::kPlusAddressOverride);
  params.menu_model = std::make_unique<TestMenuModel>(base::DoNothing());
  RunTestSequence(ShowToast(std::move(params)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  EnsurePresent(toasts::ToastView::kToastMenuButton),
                  PressButton(toasts::ToastView::kToastMenuButton),
                  WaitForShow(kSampleMenuItem),
                  EnsurePresent(toasts::ToastView::kToastViewId),
                  FireToastCloseTimer(),
                  EnsurePresent(toasts::ToastView::kToastViewId),
                  PressButton(toasts::ToastView::kToastMenuButton),
                  WaitForHide(toasts::ToastView::kToastViewId));
}

// Tests that clicking the menu button twice closes the menu, but not the toast.
IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, TwoClicksOnMenuButton) {
  ToastParams params(ToastId::kPlusAddressOverride);
  int counter = 0;
  params.menu_model = std::make_unique<TestMenuModel>(
      base::BindLambdaForTesting([&counter]() { ++counter; }));
  RunTestSequence(ShowToast(std::move(params)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  EnsurePresent(toasts::ToastView::kToastMenuButton),
                  PressButton(toasts::ToastView::kToastMenuButton),
                  WaitForShow(kSampleMenuItem),
                  PressButton(toasts::ToastView::kToastMenuButton),
                  WaitForHide(kSampleMenuItem),
                  EnsurePresent(toasts::ToastView::kToastMenuButton),
                  Check([&]() { return counter == 0; }));
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
