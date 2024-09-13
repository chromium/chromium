// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class ToastControllerInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {toast_features::kToastFramework, toast_features::kLinkCopiedToast,
         toast_features::kImageCopiedToast, toast_features::kReadingListToast},
        {});
    InteractiveBrowserTest::SetUp();
  }

  ToastController* GetToastController() {
    return browser()->browser_window_features()->toast_controller();
  }

  auto ShowToast(ToastParams params) {
    return Do(
        [&]() { GetToastController()->MaybeShowToast(std::move(params)); });
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

// TODO(crbug.com/358664193): Add tests for focus traversal using tab/shift-tab.
