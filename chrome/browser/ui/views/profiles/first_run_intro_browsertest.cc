// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error Platform not supported
#endif

// Tests for the chrome://intro WebUI page. They live here and not in the webui
// directory because they manipulate views.

class FirstRunIntroPixelTest : public UiBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    profile_picker_view_ = ProfileManagementStepTestView::CreateForStep(
        browser()->profile(), ProfileManagementFlowController::Step::kIntro,
        base::BindRepeating([](ProfilePickerWebContentsHost* host) {
          return CreateIntroStep(host, /*enable_animations=*/false);
        }));
    profile_picker_view_->ShowAndWait();
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "FirstRunIntroPixelTest", screenshot_name);
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ViewDeletedWaiter(profile_picker_view_).Wait();
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return profile_picker_view_->GetWidget();
  }

  base::test::ScopedFeatureList scoped_feature_list_{kForYouFre};
  raw_ptr<ProfileManagementStepTestView, DanglingUntriaged>
      profile_picker_view_;
};

IN_PROC_BROWSER_TEST_F(FirstRunIntroPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
