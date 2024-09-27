// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/lens/lens_permission_user_action.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {
class LensPermissionBubbleInteractiveUiTest : public InteractiveBrowserTest {
 public:
  LensPermissionBubbleInteractiveUiTest() = default;
  ~LensPermissionBubbleInteractiveUiTest() override = default;
  LensPermissionBubbleInteractiveUiTest(
      const LensPermissionBubbleInteractiveUiTest&) = delete;
  void operator=(const LensPermissionBubbleInteractiveUiTest&) = delete;

  auto* GetDialog() { return controller_->dialog_widget_for_testing(); }

  auto* GetPrefService() { return browser()->profile()->GetPrefs(); }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    controller_ = std::make_unique<lens::LensPermissionBubbleController>(
        browser(), GetPrefService(), LensOverlayInvocationSource::kAppMenu);
    request_permission_callback_called_ = false;
  }

  auto RequestPermission() {
    return Do(base::BindLambdaForTesting([&]() {
      controller_->RequestPermission(
          browser()->tab_strip_model()->GetActiveTab()->contents(),
          base::BindRepeating(
              &LensPermissionBubbleInteractiveUiTest::RequestPermissionCallback,
              base::Unretained(this)));
      EXPECT_TRUE(!!GetDialog());
      EXPECT_TRUE(GetDialog()->IsVisible());
    }));
  }

  auto CheckCancelButtonResults() {
    return Do(base::BindLambdaForTesting([&]() {
      histogram_tester.ExpectBucketCount(
          "Lens.Overlay.PermissionBubble.UserAction",
          LensPermissionUserAction::kCancelButtonPressed,
          /*expected_count=*/1);
      histogram_tester.ExpectBucketCount(
          "Lens.Overlay.PermissionBubble.ByInvocationSource.AppMenu.UserAction",
          LensPermissionUserAction::kCancelButtonPressed,
          /*expected_count=*/1);
      histogram_tester.ExpectTotalCount(
          "Lens.Overlay.PermissionBubble.UserAction", 1);
      histogram_tester.ExpectTotalCount(
          "Lens.Overlay.PermissionBubble.ByInvocationSource.AppMenu.UserAction",
          1);
      EXPECT_FALSE(!!GetDialog());
      EXPECT_FALSE(CanSharePageScreenshotWithLensOverlay(GetPrefService()));
    }));
  }

  auto CheckContinueButtonResults() {
    return Do(base::BindLambdaForTesting([&]() {
      histogram_tester.ExpectBucketCount(
          "Lens.Overlay.PermissionBubble.UserAction",
          LensPermissionUserAction::kAcceptButtonPressed,
          /*expected_count=*/1);
      histogram_tester.ExpectBucketCount(
          "Lens.Overlay.PermissionBubble.ByInvocationSource.AppMenu.UserAction",
          LensPermissionUserAction::kAcceptButtonPressed,
          /*expected_count=*/1);
      histogram_tester.ExpectTotalCount(
          "Lens.Overlay.PermissionBubble.UserAction", 1);
      histogram_tester.ExpectTotalCount(
          "Lens.Overlay.PermissionBubble.ByInvocationSource.AppMenu.UserAction",
          1);
      EXPECT_TRUE(request_permission_callback_called_);
      EXPECT_FALSE(!!GetDialog());
      EXPECT_TRUE(CanSharePageScreenshotWithLensOverlay(GetPrefService()));
    }));
  }

  void RequestPermissionCallback() {
    request_permission_callback_called_ = true;
  }

  void TearDownOnMainThread() override { controller_.reset(); }

  base::HistogramTester histogram_tester;

 private:
  base::test::ScopedFeatureList feature_list_{lens::features::kLensOverlay};
  std::unique_ptr<LensPermissionBubbleController> controller_;
  bool request_permission_callback_called_;
};

IN_PROC_BROWSER_TEST_F(LensPermissionBubbleInteractiveUiTest,
                       PermissionBubbleOpenAndCancel) {
  RunTestSequence(
      EnsureNotPresent(kLensPermissionDialogCancelButtonElementId),
      RequestPermission(), Do(base::BindLambdaForTesting([&]() {
        histogram_tester.ExpectTotalCount("Lens.Overlay.PermissionBubble.Shown",
                                          /*expected_count=*/1);
      })),
      Do(base::BindLambdaForTesting([&]() {
        histogram_tester.ExpectTotalCount(
            "Lens.Overlay.PermissionBubble.ByInvocationSource.AppMenu.Shown",
            /*expected_count=*/1);
      })),
      PressButton(kLensPermissionDialogCancelButtonElementId),
      CheckCancelButtonResults());
}

IN_PROC_BROWSER_TEST_F(LensPermissionBubbleInteractiveUiTest,
                       PermissionBubbleOpenAndContinue) {
  RunTestSequence(
      EnsureNotPresent(kLensPermissionDialogOkButtonElementId),
      RequestPermission(), Do(base::BindLambdaForTesting([&]() {
        histogram_tester.ExpectTotalCount("Lens.Overlay.PermissionBubble.Shown",
                                          /*expected_count=*/1);
      })),
      Do(base::BindLambdaForTesting([&]() {
        histogram_tester.ExpectTotalCount(
            "Lens.Overlay.PermissionBubble.ByInvocationSource.AppMenu.Shown",
            /*expected_count=*/1);
      })),
      PressButton(kLensPermissionDialogOkButtonElementId),
      CheckContinueButtonResults());
}

IN_PROC_BROWSER_TEST_F(LensPermissionBubbleInteractiveUiTest,
                       RequestPermissionMultipleTimes) {
  RunTestSequence(
      EnsureNotPresent(kLensPermissionDialogOkButtonElementId),
      RequestPermission(), Do(base::BindLambdaForTesting([&]() {
        histogram_tester.ExpectTotalCount("Lens.Overlay.PermissionBubble.Shown",
                                          /*expected_count=*/1);
      })),
      RequestPermission(), Do(base::BindLambdaForTesting([&]() {
        histogram_tester.ExpectTotalCount("Lens.Overlay.PermissionBubble.Shown",
                                          /*expected_count=*/2);
      })),
      Do(base::BindLambdaForTesting([&]() {
        histogram_tester.ExpectTotalCount(
            "Lens.Overlay.PermissionBubble.ByInvocationSource.AppMenu.Shown",
            /*expected_count=*/2);
      })));
}
}  // namespace lens
