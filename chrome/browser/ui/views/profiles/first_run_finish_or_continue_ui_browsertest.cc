// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/first_run_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/view_tracker.h"
#include "url/gurl.h"

namespace {
const std::vector<PixelTestParam>& GetTestParams() {
  static const base::NoDestructor<std::vector<PixelTestParam>> kParams({
      {.test_suffix = "LightTheme"},
      {.test_suffix = "DarkTheme", .use_dark_theme = true},
      {.test_suffix = "RightToLeftLanguage",
       .use_right_to_left_language = true},
  });
  return *kParams;
}
}  // namespace

class FirstRunFinishOrContinuePixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  FirstRunFinishOrContinuePixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam()) {
    scoped_feature_list_.InitWithFeatureStates(
        {{switches::kFirstRunDesktopRefresh, true},
         {switches::kFirstRunDesktopChoiceScreenRefresh, true},
         {switches::kFirstRunDesktopRevamp, true},
         {switches::kDisableFirstRunAnimationsForTesting, true}});
  }

  void ShowUi(const std::string& name) override {
    SignInWithAccount();

    auto* view = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kIntro,
        /*step_controller_factory=*/
        base::BindRepeating([](ProfilePickerWebContentsHost* host) {
          return CreateIntroStep(
              host, /*choice_callback=*/base::DoNothing(),
              /*enable_animations=*/false,
              /*query_effects_callback=*/base::BindRepeating([] {
                return false;
              }));
        }));
    profile_picker_view_tracker_.SetView(view);
    view->ShowAndWait();

    GURL finish_or_continue_url =
        GURL(chrome::kChromeUIIntroURL)
            .Resolve(chrome::kChromeUIIntroFinishOrContinueSubPage);
    content::TestNavigationObserver observer(view->GetPickerContents());
    view->GetPickerContents()->GetController().LoadURL(
        finish_or_continue_url, content::Referrer(),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
    observer.Wait();
  }

  bool VerifyUi() override {
    views::Widget* widget = CHECK_DEREF(profile_picker_view()).GetWidget();
    const testing::TestInfo* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    CHECK(test_info);
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "FirstRunFinishOrContinuePixelTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    if (ProfileManagementStepTestView* view = profile_picker_view()) {
      ViewDeletedWaiter(view).Wait();
    }
  }

 private:
  ProfileManagementStepTestView* profile_picker_view() {
    return static_cast<ProfileManagementStepTestView*>(
        profile_picker_view_tracker_.view());
  }

  views::ViewTracker profile_picker_view_tracker_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky: crbug.com/520054743
IN_PROC_BROWSER_TEST_P(FirstRunFinishOrContinuePixelTest,
                       DISABLED_InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FirstRunFinishOrContinuePixelTest,
    testing::ValuesIn(GetTestParams()),
    [](const testing::TestParamInfo<PixelTestParam>& info) {
      return info.param.test_suffix;
    });
