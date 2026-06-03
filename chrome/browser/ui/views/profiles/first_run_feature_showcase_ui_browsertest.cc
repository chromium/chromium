// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/first_run_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

// Tests for the chrome://feature-showcase WebUI page. They live here and
// not in the webui directory because they manipulate views.
namespace {

struct FeatureShowcaseTestParam {
  PixelTestParam pixel_test_param;
  std::string step;
};

const std::vector<FeatureShowcaseTestParam>& GetTestParams() {
  static const base::NoDestructor<std::vector<FeatureShowcaseTestParam>>
      kParams([] {
        const PixelTestParam kBaseTestParams[] = {
            {.test_suffix = "LightTheme", .window_size = gfx::Size(1024, 768)},
        };

        const std::string kSteps[] = {
            "example",
        };

        std::vector<FeatureShowcaseTestParam> params;
        for (const auto& pixel_test_param : kBaseTestParams) {
          for (const auto& step : kSteps) {
            params.push_back({pixel_test_param, step});
          }
        }
        return params;
      }());
  return *kParams;
}
}  // namespace

class FirstRunFeatureShowcasePixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<FeatureShowcaseTestParam>,
      public views::ViewObserver {
 public:
  FirstRunFeatureShowcasePixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {
    scoped_feature_list_.InitWithFeatures(
        {switches::kFirstRunDesktopRefresh,
         switches::kFirstRunDesktopChoiceScreenRefresh,
         switches::kFirstRunDesktopRevamp},
        {});
  }

  ~FirstRunFeatureShowcasePixelTest() override {
    if (profile_picker_view_) {
      profile_picker_view_->views::View::RemoveObserver(this);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ProfilesPixelTestBaseT<UiBrowserTest>::SetUpCommandLine(command_line);

    CHECK(!GetParam().step.empty());
    command_line->AppendSwitchASCII(switches::kForceFreFeatureShowcaseSteps,
                                    GetParam().step);
  }

  void ShowUi(const std::string& name) override {
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::NONE);

    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kFeatureShowcase,
        /*step_controller_factory=*/
        base::BindRepeating(
            [](Profile* profile, ProfilePickerWebContentsHost* host)
                -> std::unique_ptr<ProfileManagementStepController> {
              return CreateFeatureShowcaseStep(host, profile,
                                               base::DoNothing());
            },
            browser()->profile()));

    profile_picker_view_->views::View::AddObserver(this);
    profile_picker_view_->ShowAndWait(GetParam().pixel_test_param.window_size);
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    const testing::TestInfo* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    CHECK(test_info);
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name(), "_",
                      GetParam().step});

    return VerifyPixelUi(widget, "FirstRunFeatureShowcasePixelTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    if (!profile_picker_view_) {
      return;
    }
    DCHECK(GetWidgetForScreenshot());
    ViewDeletedWaiter(profile_picker_view_).Wait();
  }

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    profile_picker_view_ = nullptr;
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    if (!profile_picker_view_) {
      return nullptr;
    }
    return profile_picker_view_->GetWidget();
  }

  raw_ptr<ProfileManagementStepTestView> profile_picker_view_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
  gfx::ScopedAnimationDurationScaleMode disable_animations_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

// TODO(crbug.com/519129009): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif
IN_PROC_BROWSER_TEST_P(FirstRunFeatureShowcasePixelTest,
                       MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FirstRunFeatureShowcasePixelTest,
    testing::ValuesIn(GetTestParams()),
    [](const testing::TestParamInfo<FeatureShowcaseTestParam>& info) {
      return info.param.pixel_test_param.test_suffix + "_" + info.param.step;
    });
