// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

class ToastControllerInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {toast_features::kToastFramework, toast_features::kLinkCopiedToast,
         toast_features::kImageCopiedToast},
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, ShowEphemeralToast) {
  RunTestSequence(ShowToast(ToastParams(ToastId::kLinkCopied)),
                  WaitForShow(toasts::ToastView::kToastViewId), Check([=]() {
                    return GetToastController()->IsShowingToast();
                  }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest,
                       ShowSameEphemeralToastTwice) {
  RunTestSequence(
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=]() { return GetToastController()->IsShowingToast(); }),
      ShowToast(ToastParams(ToastId::kLinkCopied)),
      WaitForShow(toasts::ToastView::kToastViewId),
      Check([=]() { return GetToastController()->IsShowingToast(); }));
}

IN_PROC_BROWSER_TEST_F(ToastControllerInteractiveTest, PreemptEphemeralToast) {
  RunTestSequence(ShowToast(ToastParams(ToastId::kLinkCopied)),
                  WaitForShow(toasts::ToastView::kToastViewId), Check([=]() {
                    return GetToastController()->IsShowingToast();
                  }),
                  ShowToast(ToastParams(ToastId::kImageCopied)));
}
