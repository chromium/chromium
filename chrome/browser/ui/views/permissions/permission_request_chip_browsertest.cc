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
#include "chrome/browser/ui/views/permissions/permission_chip.h"
#include "chrome/browser/ui/views/permissions/permission_request_chip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/permissions/features.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/animation/animation_test_api.h"

namespace {

void RequestPermission(Browser* browser) {
  test::PermissionRequestManagerTestApi test_api(browser);
  permissions::PermissionRequestObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());

  EXPECT_NE(nullptr, test_api.manager());
  test_api.AddSimpleRequest(
      browser->tab_strip_model()->GetActiveWebContents()->GetPrimaryMainFrame(),
      permissions::RequestType::kGeolocation);

  observer.Wait();
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
                       ChipFinalizedWhenInteractingWithOmnibox) {
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
  EXPECT_TRUE(lbv->chip()->IsInitialized());

  // Type something in the omnibox.
  auto* omnibox_view = lbv->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  omnibox_view->model()->SetInputInProgress(true);

  base::RunLoop().RunUntilIdle();

  // While the user is interacting with the omnibox, the chip is hidden, the
  // location icon isn't offset by the chip and the bubble is hidden.
  EXPECT_FALSE(lbv->chip()->GetVisible());
  EXPECT_FALSE(lbv->chip()->IsBubbleShowing());
  EXPECT_FALSE(lbv->chip()->IsInitialized());
  EXPECT_EQ(lbv->location_icon_view()->bounds().x(),
            GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING));
}

IN_PROC_BROWSER_TEST_F(PermissionRequestChipBrowserTest,
                       ChipIsNotShownWhenInteractingWithOmnibox) {
  LocationBarView* lbv = GetLocationBarView(browser());

  // The chip is not shown because there is no active permission request.
  EXPECT_FALSE(lbv->chip()->GetVisible());

  // Type something in the omnibox.
  auto* omnibox_view = lbv->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  omnibox_view->model()->SetInputInProgress(true);

  RequestPermission(browser());

  // While the user is interacting with the omnibox, an incoming permission
  // request will be automatically ignored. The chip is not shown.
  EXPECT_FALSE(lbv->chip()->GetVisible());
}

// This is an end-to-end test that verifies that a permission prompt bubble will
// not be shown because of the empty address bar. Under the normal conditions
// such a test should be placed in PermissionsSecurityModelInteractiveUITest but
// due to dependency issues (see crbug.com/1112591) `//chrome/browser` is not
// allowed to have dependencies on `//chrome/browser/ui/views/*`.
IN_PROC_BROWSER_TEST_F(PermissionRequestChipBrowserTest,
                       PermissionRequestIsAutoIgnored) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  content::WebContents::FromRenderFrameHost(main_rfh)->Focus();

  ASSERT_TRUE(main_rfh);

  constexpr char kCheckMicrophone[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'microphone'});
      resolve(PermissionStatus.state === 'granted');
    })
    )";

  constexpr char kRequestMicrophone[] = R"(
    new Promise(async resolve => {
      var constraints = { audio: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  LocationBarView* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->location_bar();

  // Type something in the omnibox.
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  omnibox_view->model()->SetInputInProgress(true);

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  permissions::PermissionRequestObserver observer(embedder_contents);

  EXPECT_FALSE(manager->IsRequestInProgress());

  EXPECT_TRUE(content::ExecJs(
      main_rfh, kRequestMicrophone,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait until a permission request is shown or finalized.
  observer.Wait();

  // Permission request was finalized without showing a prompt bubble.
  EXPECT_FALSE(manager->IsRequestInProgress());
  EXPECT_FALSE(observer.request_shown());

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
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
