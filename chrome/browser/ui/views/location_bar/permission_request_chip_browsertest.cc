// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/permission_request_chip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/permissions/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/animation/animation_test_api.h"

namespace {

void RequestPermission(Browser* browser) {
  test::PermissionRequestManagerTestApi test_api(browser);
  EXPECT_NE(nullptr, test_api.manager());
  test_api.AddSimpleRequest(
      browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      permissions::RequestType::kGeolocation);

  base::RunLoop().RunUntilIdle();
}

LocationBarView* GetLocationBarView(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar()
      ->location_bar();
}

}  // namespace

class PermissionRequestChipBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {permissions::features::kPermissionChip,
         permissions::features::kPermissionChipGestureSensitive,
         permissions::features::kPermissionChipRequestTypeSensitive},
        {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestChipBrowserTest,
                       ChipHiddenWhenInteractingWithOmnibox) {
  RequestPermission(browser());
  LocationBarView* lbv = GetLocationBarView(browser());
  auto* button = static_cast<OmniboxChipButton*>(lbv->chip()->button());
  auto* animation = button->animation_for_testing();

  // Animate the chip expand.
  gfx::AnimationTestApi animation_api(animation);
  base::TimeTicks now = base::TimeTicks::Now();
  animation_api.SetStartTime(now);
  animation_api.Step(now + animation->GetSlideDuration());

  // After animation ended, the chip is expanded and the bubble is shown.
  EXPECT_TRUE(lbv->chip()->GetVisible());
  EXPECT_TRUE(lbv->chip()->IsBubbleShowing());

  // Type something in the omnibox.
  auto* omnibox_view = lbv->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  omnibox_view->model()->SetInputInProgress(true);

  base::RunLoop().RunUntilIdle();

  // While the user is interacting with the omnibox, the chip is hidden, the
  // location icon isn't offset by the chip and the bubble is hidden.
  EXPECT_FALSE(lbv->chip()->GetVisible());
  EXPECT_FALSE(lbv->chip()->IsBubbleShowing());
  EXPECT_EQ(lbv->location_icon_view()->bounds().x(),
            GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING));
}

class PermissionRequestChipDialogBrowserTest : public UiBrowserTest {
 public:
  PermissionRequestChipDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(permissions::features::kPermissionChip);
  }

  PermissionRequestChipDialogBrowserTest(
      const PermissionRequestChipDialogBrowserTest&) = delete;
  PermissionRequestChipDialogBrowserTest& operator=(
      const PermissionRequestChipDialogBrowserTest&) = delete;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    RequestPermission(browser());

    LocationBarView* lbv = GetLocationBarView(browser());
    lbv->GetFocusManager()->ClearFocus();
    auto* button = static_cast<OmniboxChipButton*>(lbv->chip()->button());
    button->SetForceExpandedForTesting(true);
  }

  bool VerifyUi() override {
    LocationBarView* lbv = GetLocationBarView(browser());
    PermissionChip* chip = lbv->chip();
    if (!chip)
      return false;

// TODO(olesiamrukhno): VerifyPixelUi works only for these platforms, revise
// this if supported platforms change.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});
    return VerifyPixelUi(chip, "BrowserUi", screenshot_name);
#else
    return true;
#endif
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Temporarily disabled per https://crbug.com/1197280
IN_PROC_BROWSER_TEST_F(PermissionRequestChipDialogBrowserTest,
                       DISABLED_InvokeUi_geolocation) {
  ShowAndVerifyUi();
}
