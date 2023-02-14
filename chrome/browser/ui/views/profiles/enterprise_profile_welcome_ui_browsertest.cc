// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/any_widget_observer.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error Platform not supported
#endif

// Tests for the chrome://enterprise-profile-welcome/ WebUI page. They live here
// and not in the webui directory because they manipulate views.

namespace {
struct EnterpriseWelcomeTestParam {
  PixelTestParam pixel_test_param;
  bool profile_creation_required_by_policy = false;
  bool show_link_data_checkbox = false;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<EnterpriseWelcomeTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const EnterpriseWelcomeTestParam kWindowTestParams[] = {
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcome"}},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeDarkTheme",
                          .use_dark_theme = true}},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeRtl",
                          .use_right_to_left_language = true}},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeSmallWindow",
                          .use_small_window = true}},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeFre",
                          .use_fre_style = true}},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeFreDarkTheme",
                          .use_dark_theme = true,
                          .use_fre_style = true}},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeFreRtl",
                          .use_right_to_left_language = true,
                          .use_fre_style = true}},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeFreSmallWindow",
                          .use_small_window = true,
                          .use_fre_style = true}},
};

const EnterpriseWelcomeTestParam kDialogTestParams[] = {
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcome"}},
    {.pixel_test_param = {.test_suffix =
                              "EnterpriseWelcomeWithLinkDataCheckbox"},
     .show_link_data_checkbox = true},
    {.pixel_test_param = {.test_suffix =
                              "EnterpriseWelcomeWithProfileCreationRequired"},
     .profile_creation_required_by_policy = true},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeDarkTheme",
                          .use_dark_theme = true},
     .show_link_data_checkbox = true},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeRtl",
                          .use_right_to_left_language = true},
     .show_link_data_checkbox = true},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeFre",
                          .use_fre_style = true}},
    {.pixel_test_param = {.test_suffix =
                              "EnterpriseWelcomeFreWithLinkDataCheckbox",
                          .use_fre_style = true},
     .show_link_data_checkbox = true},
    {.pixel_test_param =
         {.test_suffix = "EnterpriseWelcomeFreWithProfileCreationRequired"},
     .profile_creation_required_by_policy = true},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeFreDarkTheme",
                          .use_dark_theme = true,
                          .use_fre_style = true},
     .show_link_data_checkbox = true},
    {.pixel_test_param = {.test_suffix = "EnterpriseWelcomeFreRtl",
                          .use_right_to_left_language = true,
                          .use_fre_style = true},
     .show_link_data_checkbox = true},
};

// Creates a step to represent the enterprise-profile-welcome
class EnterpriseWelcomeStepControllerForTest
    : public ProfileManagementStepController {
 public:
  explicit EnterpriseWelcomeStepControllerForTest(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      const AccountInfo& account_info)
      : ProfileManagementStepController(host),
        enterprise_welcome_url_(
            GURL(chrome::kChromeUIEnterpriseProfileWelcomeURL)),
        profile_(profile),
        account_info_(&account_info) {}

  ~EnterpriseWelcomeStepControllerForTest() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    // Reload the WebUI in the picker contents.
    host()->ShowScreenInPickerContents(
        enterprise_welcome_url_,
        base::BindOnce(
            &EnterpriseWelcomeStepControllerForTest::OnEnterpriseWelcomeLoaded,
            weak_ptr_factory_.GetWeakPtr(), std::move(step_shown_callback)));
  }

  void OnNavigateBackRequested() override { NOTREACHED(); }

  void OnEnterpriseWelcomeLoaded(
      StepSwitchFinishedCallback step_shown_callback) {
    DCHECK(profile_);
    DCHECK(account_info_);
    auto* enterprise_welcome_ui = static_cast<EnterpriseProfileWelcomeUI*>(
        host()->GetPickerContents()->GetWebUI()->GetController());

    enterprise_welcome_ui->Initialize(
        /*browser=*/nullptr,
        EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled,
        *account_info_, /*profile_creation_required_by_policy=*/false,
        /*show_link_data_option=*/false, /*profile_color=*/absl::nullopt,
        /*proceed_callback*/ base::DoNothing());

    if (step_shown_callback) {
      std::move(step_shown_callback).Run(/*success=*/true);
    }
  }

 private:
  const GURL enterprise_welcome_url_;
  raw_ptr<Profile> profile_;
  raw_ptr<const AccountInfo> account_info_;
  base::WeakPtrFactory<EnterpriseWelcomeStepControllerForTest>
      weak_ptr_factory_{this};
};
}  // namespace

class EnterpriseWelcomeUIWindowPixelTest
    : public UiBrowserTest,
      public testing::WithParamInterface<EnterpriseWelcomeTestParam> {
 public:
  EnterpriseWelcomeUIWindowPixelTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};
    InitPixelTestFeatures(GetParam().pixel_test_param, scoped_feature_list_,
                          enabled_features, disabled_features);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetUpPixelTestCommandLine(GetParam().pixel_test_param, scoped_env_override_,
                              command_line);
  }

  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    DCHECK(browser());

    auto account_info = SignInWithPrimaryAccount(
        browser()->profile(), AccountManagementStatus::kManaged);
    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kPostSignInFlow,
        /*step_controller_factory=*/
        base::BindLambdaForTesting(
            [this, &account_info](ProfilePickerWebContentsHost* host)
                -> std::unique_ptr<ProfileManagementStepController> {
              return std::make_unique<EnterpriseWelcomeStepControllerForTest>(
                  host, browser()->profile(), account_info);
            }));
    profile_picker_view_->ShowAndWait(
        GetParam().pixel_test_param.use_small_window
            ? absl::optional<gfx::Size>(gfx::Size(750, 590))
            : absl::nullopt);
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "EnterpriseWelcomeUIWindowPixelTest",
                         screenshot_name);
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ViewDeletedWaiter(profile_picker_view_).Wait();
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return profile_picker_view_->GetWidget();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<ProfileManagementStepTestView, DanglingUntriaged>
      profile_picker_view_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> scoped_env_override_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseWelcomeUIWindowPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseWelcomeUIWindowPixelTest,
                         testing::ValuesIn(kWindowTestParams),
                         &ParamToTestSuffix);

class EnterpriseWelcomeUIDialogPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<EnterpriseWelcomeTestParam> {
 public:
  EnterpriseWelcomeUIDialogPixelTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};
    InitPixelTestFeatures(GetParam().pixel_test_param, scoped_feature_list_,
                          enabled_features, disabled_features);
  }

  ~EnterpriseWelcomeUIDialogPixelTest() override = default;

  void ShowUi(const std::string& name) override {
    DCHECK(browser());

    auto account_info = SignInWithPrimaryAccount(
        browser()->profile(), AccountManagementStatus::kManaged);
    auto url = GURL(chrome::kChromeUIEnterpriseProfileWelcomeURL);

    // Wait for the web content to load to be able to properly render the
    // modal dialog.
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();

    // ShowUi() can sometimes return before the dialog widget is shown because
    // the call to show the latter is asynchronous. Adding
    // NamedWidgetShownWaiter will prevent that from happening.
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");

    auto* controller = browser()->signin_view_controller();
    controller->ShowModalEnterpriseConfirmationDialog(
        account_info, GetParam().profile_creation_required_by_policy,
        GetParam().show_link_data_checkbox,
        GetDefaultProfileThemeColors().profile_highlight_color,
        base::DoNothing());

    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetUpPixelTestCommandLine(GetParam().pixel_test_param, scoped_env_override_,
                              command_line);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> scoped_env_override_;
};

IN_PROC_BROWSER_TEST_P(EnterpriseWelcomeUIDialogPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseWelcomeUIDialogPixelTest,
                         testing::ValuesIn(kDialogTestParams),
                         &ParamToTestSuffix);
