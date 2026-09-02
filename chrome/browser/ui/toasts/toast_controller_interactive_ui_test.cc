// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);

using ToastViewObserver =
    views::test::PollingViewObserver<bool, toasts::ToastView>;
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ToastViewObserver, kToastViewObserver);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kBookmarkStarFocused);

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

class ToastControllerInteractiveTest
    : public SplitViewInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    SplitViewInteractiveTestMixin::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{toast_features::kLinkCopiedToast, {}},
            {toast_features::kImageCopiedToast, {}},
            {toast_features::kReadingListToast, {}}};
  }

  GURL GetURL(std::string_view hostname = "example.com",
              std::string_view path = "/title1.html") {
    return embedded_test_server()->GetURL(hostname, path);
  }

  ToastController* GetToastController() {
    return ToastController::From(browser());
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

  auto WaitForToastView(
      base::RepeatingCallback<bool(const toasts::ToastView*)> check) {
    auto result = Steps(
        PollView(kToastViewObserver, toasts::ToastView::kToastViewId, check),
        WaitForState(kToastViewObserver, true),
        StopObservingState(kToastViewObserver));
    AddDescriptionPrefix(result, "WaitForToastView()");
    return result;
  }

  auto WaitForBookmarkStar() {
    return Steps(PollState(kBookmarkStarFocused,
                           [this]() {
                             page_actions::PageActionTestAccessor accessor(
                                 browser(), kActionBookmarkThisTab);
                             return accessor.HasFocus();
                           }),
                 WaitForState(kBookmarkStarFocused, true),
                 StopObservingState(kBookmarkStarFocused));
  }

  template <typename T, typename U>
  auto TestToastFocus(T&& check_previous_focus, U&& check_next_focus) {
    ui::Accelerator next_pane;
    EXPECT_TRUE(
        BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
            IDC_FOCUS_NEXT_PANE, &next_pane));
    return Steps(
        ShowToast(ToastParams(ToastId::kAddedToReadingList)),
        WaitForShow(toasts::ToastView::kToastViewId),
        ActivateSurface(toasts::ToastView::kToastViewId),
        SendAccelerator(kBrowserViewElementId, next_pane),
        WaitForToastView(
            base::BindRepeating([](const toasts::ToastView* toast) {
              return toast->action_button_for_testing()->HasFocus();
            })),
        // Advancing focus backwards should move out of the toast
        AdvanceKeyboardFocus(true),
        WaitForToastView(
            base::BindRepeating([](const toasts::ToastView* toast) {
              return !toast->action_button_for_testing()->HasFocus();
            })),
        std::move(check_previous_focus),
        // Advancing focus should bring us back into the toast
        AdvanceKeyboardFocus(false),
        WaitForToastView(
            base::BindRepeating([](const toasts::ToastView* toast) {
              return toast->action_button_for_testing()->HasFocus();
            })),
        AdvanceKeyboardFocus(false),
        WaitForToastView(
            base::BindRepeating([](const toasts::ToastView* toast) {
              return toast->close_button_for_testing()->HasFocus();
            })),
        // Advancing focus again should move out of the toast
        AdvanceKeyboardFocus(false),
        WaitForToastView(
            base::BindRepeating([](const toasts::ToastView* toast) {
              return !toast->close_button_for_testing()->HasFocus();
            })),
        std::move(check_next_focus),
        // Advancing focus backwards should bring us back into the toast
        AdvanceKeyboardFocus(true),
        WaitForToastView(
            base::BindRepeating([](const toasts::ToastView* toast) {
              return toast->close_button_for_testing()->HasFocus();
            })));
  }

  void RemoveOmniboxFocus() {
    BrowserView::GetBrowserViewForBrowser(browser())
        ->GetFocusManager()
        ->ClearFocus();
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
      WaitForState(views::test::kCurrentWidgetFocus, std::ref(toast_widget)),
      CheckView(toasts::ToastView::kToastViewId, [](toasts::ToastView* toast) {
        return toast->GetFocusManager()->GetFocusedView() ==
               toast->action_button_for_testing();
      }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, FocusTraversal) {
  RunTestSequence(TestToastFocus(
#if BUILDFLAG(IS_MAC)
      // Mac focus traversal order is slightly different from other platforms
      CheckView(kToolbarAppMenuButtonElementId,
                [](AppMenuButton* button) { return button->HasFocus(); }),
#else

      WaitForBookmarkStar(),
#endif
      CheckView(kBrowserViewElementId, [](BrowserView* browser_view) {
        return browser_view->GetActiveContentsWebView()->HasFocus();
      })));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       FocusTraversalSplitStart) {
  ui::Accelerator next_pane;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_PANE, &next_pane));
  RunTestSequence(
      // Set up split view with first tab focused
      AddInstrumentedTab(kSecondTab, GetURL()),
      SplitViewInteractiveTestMixin::EnterSplitView(0, 1),
      CheckResult([=, this]() { return tab_strip_model()->active_index(); }, 0),
      TestToastFocus(
#if BUILDFLAG(IS_MAC)
          // Mac focus traversal order is slightly different from other
          // platforms
          CheckView(kToolbarAppMenuButtonElementId,
                    [](AppMenuButton* button) { return button->HasFocus(); }),
#else
          WaitForBookmarkStar(),
#endif
          Steps(
              CheckResult(
                  [=, this]() { return tab_strip_model()->active_index(); }, 0),
              CheckView(kBrowserViewElementId, [](BrowserView* browser_view) {
                return browser_view->GetActiveContentsWebView()->HasFocus();
              }))));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, FocusTraversalSplitEnd) {
  ui::Accelerator next_pane;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_PANE, &next_pane));
  RunTestSequence(
      // Set up split view with second tab focused
      AddInstrumentedTab(kSecondTab, GetURL()),
      SelectTab(kTabStripElementId, 1),
      SplitViewInteractiveTestMixin::EnterSplitView(1, 0),
      CheckResult([=, this]() { return tab_strip_model()->active_index(); }, 1),
      TestToastFocus(
          CheckView(
              MultiContentsResizeHandle::kMultiContentsResizeHandleElementId,
              [](MultiContentsResizeHandle* resize_handle) {
                return resize_handle->HasFocus();
              }),
          Steps(
              CheckResult(
                  [=, this]() { return tab_strip_model()->active_index(); }, 1),
              CheckView(kBrowserViewElementId, [](BrowserView* browser_view) {
                return browser_view->GetActiveContentsWebView()->HasFocus();
              }))));
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
                  EnsurePresent(toasts::ToastView::kToastViewId),
                  // Explicitly close the persistent toast to prevent the widget
                  // from leaking into browser teardown.
                  Do([this]() {
                    GetToastController()->GetToastWidgetForTesting()->Close();
                  }),
                  WaitForHide(toasts::ToastView::kToastViewId));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       NavigationPersistentToastStaysOnNavigation) {
  ToastParams params(ToastId::kEmailVerified);
  params.body_string_replacement_params = {u"dummy"};
  params.menu_model = std::make_unique<TestMenuModel>(base::DoNothing());
  RunTestSequence(InstrumentTab(kFirstTab), ShowToast(std::move(params)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  NavigateWebContents(kFirstTab, GetURL()),
                  CheckShowingToastId(ToastId::kEmailVerified),
                  // Explicitly fire the close timer to prevent the persistent
                  // toast widget from leaking into browser teardown.
                  FireToastCloseTimer(),
                  WaitForHide(toasts::ToastView::kToastViewId));
}

// Tests that setting a menu model in `ToastParams` adds a menu button to the
// toast that runs the menu model and that interacting with a menu element
// closes the toast.
IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       MenuButtonClickOpensMenu) {
  ToastParams params(ToastId::kEmailVerified);
  params.body_string_replacement_params = {u"dummy"};
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
// closes via Escape key.
IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       ToastDoesNotCloseWhileMenuIsOpen_Escape) {
  ToastParams params(ToastId::kEmailVerified);
  params.body_string_replacement_params = {u"dummy"};
  params.menu_model = std::make_unique<TestMenuModel>(base::DoNothing());
  RunTestSequence(ShowToast(std::move(params)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  EnsurePresent(toasts::ToastView::kToastMenuButton),
                  PressButton(toasts::ToastView::kToastMenuButton),
                  WaitForShow(kSampleMenuItem),
                  EnsurePresent(toasts::ToastView::kToastViewId),
                  FireToastCloseTimer(),
                  EnsurePresent(toasts::ToastView::kToastViewId),
                  SendKeyPress(kBrowserViewElementId, ui::VKEY_ESCAPE),
                  WaitForHide(toasts::ToastView::kToastViewId));
}

// Tests that attempting to close the `ToastView` does not succeed while the
// menu is open. If that happens, the `ToastView` is closed once the menu
// closes via clicking the menu button again.
// TODO(crbug.com/398296825): Flaky on Windows builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ToastDoesNotCloseWhileMenuIsOpen_Mouse \
  DISABLED_ToastDoesNotCloseWhileMenuIsOpen_Mouse
#else
#define MAYBE_ToastDoesNotCloseWhileMenuIsOpen_Mouse \
  ToastDoesNotCloseWhileMenuIsOpen_Mouse
#endif
IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       MAYBE_ToastDoesNotCloseWhileMenuIsOpen_Mouse) {
#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Flaky in Wayland due to way events are routed and bounds "
                    "are reported";
  }
#endif
  ToastParams params(ToastId::kEmailVerified);
  params.body_string_replacement_params = {u"dummy"};
  params.menu_model = std::make_unique<TestMenuModel>(base::DoNothing());
  RunTestSequence(ShowToast(std::move(params)),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  EnsurePresent(toasts::ToastView::kToastMenuButton),
                  PressButton(toasts::ToastView::kToastMenuButton),
                  WaitForShow(kSampleMenuItem),
                  EnsurePresent(toasts::ToastView::kToastViewId),
                  FireToastCloseTimer(),
                  EnsurePresent(toasts::ToastView::kToastViewId),
                  MoveMouseTo(toasts::ToastView::kToastMenuButton),
                  ClickMouse(), WaitForHide(toasts::ToastView::kToastViewId));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       ToastReactToOmniboxFocus) {
  LocationBar* const location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBar();
  ASSERT_TRUE(location_bar);
  OmniboxView* const omnibox_view = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);
  BrowserView::GetBrowserViewForBrowser(browser())->SetFocusToLocationBar(true);
  ASSERT_FALSE(location_bar->GetOmniboxController()->IsPopupOpen());

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
  BrowserView::GetBrowserViewForBrowser(browser())->SetFocusToLocationBar(true);
  EXPECT_TRUE(toast_controller->IsShowingToast());
  // ... that may happen asynchronously, however.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return toast_controller->GetToastWidgetForTesting()->IsVisible() == false;
  }));
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
  LocationBar* const location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBar();
  ASSERT_TRUE(location_bar);
  OmniboxView* const omnibox_view = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);
  ASSERT_FALSE(location_bar->GetOmniboxController()->IsPopupOpen());
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"hello world");
  omnibox_view->OnAfterPossibleChange(true);

  ASSERT_TRUE(location_bar->GetOmniboxController()->IsPopupOpen());

  // The toast widget should no longer be visible because there is a popup.
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_FALSE(toast_controller->GetToastWidgetForTesting()->IsVisible());

  // Toast widget is visible again after the omnibox is no longer focused.
  RemoveOmniboxFocus();
  ASSERT_FALSE(location_bar->GetOmniboxController()->IsPopupOpen());
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_TRUE(toast_controller->GetToastWidgetForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       HidesWhenTypingInOmnibox) {
  BrowserView::GetBrowserViewForBrowser(browser())->SetFocusToLocationBar(true);

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
      browser()
          ->GetFeatures()
          .exclusive_access_manager()
          ->fullscreen_controller();
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
          ->GetActiveContentsWebView()
          ->GetBoundsInScreen();
  EXPECT_TRUE(web_view_bounds.Contains(toast_bounds));
}

// Regression test for http://crbug.com/383898425
IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       HandlesReducedAnimation) {
  ToastController* const toast_controller = GetToastController();
  {
    // Show toast while rich animations are disabled.
    const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations =
        gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
    gfx::Animation::SetPrefersReducedMotionForTesting(true);
    EXPECT_TRUE(gfx::Animation::PrefersReducedMotion());
    EXPECT_TRUE(
        toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  }
  {
    // Animate out toast (due to new toast showing) after enabling animations.
    const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations =
        gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
    EXPECT_TRUE(
        toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  }
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       ShowPinnedTabToastOnTabCloseViaKeyboardShortcut) {
  ui::Accelerator close_tab_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_CLOSE_TAB, &close_tab_accelerator));

  RunTestSequence(
      // Add a pinned tab.
      InstrumentTab(kFirstTab), WaitForShow(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetURL()),
      SelectTab(kTabStripElementId, 0),
      Do([&]() { browser()->tab_strip_model()->SetTabPinned(0, true); }),
      // Expect that closing the tab with an accelerator will show a toast.
      SendAccelerator(kBrowserViewElementId, close_tab_accelerator),
      WaitForShow(toasts::ToastView::kToastViewId),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2),
      // Expect that we can close the tab by pressing the accelerator again.
      SendAccelerator(kBrowserViewElementId, close_tab_accelerator),
      WaitForHide(toasts::ToastView::kToastViewId),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1));
}
