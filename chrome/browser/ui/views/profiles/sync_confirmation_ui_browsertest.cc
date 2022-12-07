// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/scoped_environment_variable_override.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error Platform not supported
#endif

// TODO(crbug.com/1374702): Move this file next to sync_confirmation_ui.cc.
// Render the page in a browser instead of a profile_picker_view to be able to
// do so.

// Tests for the chrome://sync-confirmation WebUI page. They live here and not
// in the webui directory because they manipulate views.
namespace {
struct TestParam {
  std::string test_suffix = "";
  bool use_dark_theme = false;
  bool use_tangible_sync = false;
  bool use_right_to_left_language = false;
  SyncConfirmationStyle sync_style = SyncConfirmationStyle::kWindow;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

// Permutations of supported parameters.
const TestParam kWindowTestParams[] = {
    {.test_suffix = "LegacySync"},
    {.test_suffix = "LegacySyncDarkTheme", .use_dark_theme = true},
    {.test_suffix = "LegacySyncRtl", .use_right_to_left_language = true},
    {.test_suffix = "TangibleSync", .use_tangible_sync = true},
    {.test_suffix = "TangibleSyncDarkTheme",
     .use_dark_theme = true,
     .use_tangible_sync = true},
    {.test_suffix = "TangibleSyncRtl",
     .use_tangible_sync = true,
     .use_right_to_left_language = true},
};

const TestParam kDialogTestParams[] = {
    {.test_suffix = "LegacySync",
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.test_suffix = "LegacySyncSigninInterceptStyle",
     .sync_style = SyncConfirmationStyle::kSigninInterceptModal},
    {.test_suffix = "LegacySyncDarkTheme",
     .use_dark_theme = true,
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.test_suffix = "LegacySyncRtl",
     .use_right_to_left_language = true,
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.test_suffix = "TangibleSync",
     .use_tangible_sync = true,
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.test_suffix = "TangibleSyncSigninInterceptStyle",
     .use_tangible_sync = true,
     .sync_style = SyncConfirmationStyle::kSigninInterceptModal},
    {.test_suffix = "TangibleSyncDarkTheme",
     .use_dark_theme = true,
     .use_tangible_sync = true,
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.test_suffix = "TangibleSyncRtl",
     .use_tangible_sync = true,
     .use_right_to_left_language = true,
     .sync_style = SyncConfirmationStyle::kDefaultModal},
};

GURL BuildSyncConfirmationWindowURL() {
  std::string url_string = chrome::kChromeUISyncConfirmationURL;
  return AppendSyncConfirmationQueryParams(GURL(url_string),
                                           SyncConfirmationStyle::kWindow);
}

AccountInfo FillAccountInfo(const CoreAccountInfo& core_info) {
  AccountInfo account_info;
  account_info.email = core_info.email;
  account_info.gaia = core_info.gaia;
  account_info.account_id = core_info.account_id;
  account_info.is_under_advanced_protection =
      core_info.is_under_advanced_protection;
  account_info.full_name = "Test Full Name";
  account_info.given_name = "Joe";
  account_info.hosted_domain = kNoHostedDomainFound;
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  return account_info;
}

void SignInWithPrimaryAccount(Profile* profile) {
  DCHECK(profile);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto core_account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "joe.consumer@gmail.com",
      signin::ConsentLevel::kSignin);
  auto account_info = FillAccountInfo(core_account_info);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

// Creates a step to represent the sync-confirmation.
class SyncConfirmationStepControllerForTest
    : public ProfileManagementStepController {
 public:
  explicit SyncConfirmationStepControllerForTest(
      ProfilePickerWebContentsHost* host)
      : ProfileManagementStepController(host),
        sync_confirmation_url_(BuildSyncConfirmationWindowURL()) {}

  ~SyncConfirmationStepControllerForTest() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    // Reload the WebUI in the picker contents.
    host()->ShowScreenInPickerContents(
        sync_confirmation_url_,
        base::BindOnce(
            &SyncConfirmationStepControllerForTest::OnSyncConfirmationLoaded,
            weak_ptr_factory_.GetWeakPtr(), std::move(step_shown_callback)));
  }

  void OnNavigateBackRequested() override { NOTREACHED(); }

  void OnSyncConfirmationLoaded(
      StepSwitchFinishedCallback step_shown_callback) {
    SyncConfirmationUI* sync_confirmation_ui = static_cast<SyncConfirmationUI*>(
        host()->GetPickerContents()->GetWebUI()->GetController());

    sync_confirmation_ui->InitializeMessageHandlerWithBrowser(nullptr);

    if (step_shown_callback) {
      std::move(step_shown_callback).Run(/*success=*/true);
    }
  }

 private:
  const GURL sync_confirmation_url_;
  base::WeakPtrFactory<SyncConfirmationStepControllerForTest> weak_ptr_factory_{
      this};
};

void InitFeatures(const TestParam& params,
                  base::test::ScopedFeatureList& feature_list) {
  std::vector<base::test::FeatureRef> enabled_features = {};
  if (params.use_tangible_sync) {
    enabled_features.push_back(switches::kTangibleSync);
  }
  if (params.use_dark_theme) {
    enabled_features.push_back(features::kWebUIDarkMode);
  }
  if (params.sync_style == SyncConfirmationStyle::kSigninInterceptModal) {
    enabled_features.push_back(kSyncPromoAfterSigninIntercept);
  }
  feature_list.InitWithFeatures(enabled_features, {});
}

void SetUpCommandLine(
    const TestParam& params,
    std::unique_ptr<base::ScopedEnvironmentVariableOverride>& env_variables,
    base::CommandLine* command_line) {
  if (params.use_dark_theme) {
    command_line->AppendSwitch(switches::kForceDarkMode);
  }
  if (params.use_right_to_left_language) {
    command_line->AppendSwitchASCII(switches::kLang, "ar");

    // On Linux & Lacros the command line switch has no effect, we need to use
    // environment variables to change the language.
    env_variables = std::make_unique<base::ScopedEnvironmentVariableOverride>(
        "LANGUAGE", "ar");
  }
}
}  // namespace

class SyncConfirmationUIWindowPixelTest
    : public UiBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  SyncConfirmationUIWindowPixelTest() {
    DCHECK(GetParam().sync_style == SyncConfirmationStyle::kWindow);
    InitFeatures(GetParam(), scoped_feature_list_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::SetUpCommandLine(GetParam(), scoped_env_override_, command_line);
  }

  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    DCHECK(browser());

    SignInWithPrimaryAccount(browser()->profile());
    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kPostSignInFlow,
        /*step_controller_factory=*/
        base::BindRepeating([](ProfilePickerWebContentsHost* host) {
          return std::unique_ptr<ProfileManagementStepController>(
              new SyncConfirmationStepControllerForTest(host));
        }));
    profile_picker_view_->ShowAndWait();
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "SyncConfirmationUIWindowPixelTest",
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

IN_PROC_BROWSER_TEST_P(SyncConfirmationUIWindowPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncConfirmationUIWindowPixelTest,
                         testing::ValuesIn(kWindowTestParams),
                         &ParamToTestSuffix);

class SyncConfirmationUIDialogPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  SyncConfirmationUIDialogPixelTest() {
    DCHECK(GetParam().sync_style != SyncConfirmationStyle::kWindow);
    InitFeatures(GetParam(), scoped_feature_list_);
  }

  ~SyncConfirmationUIDialogPixelTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    DCHECK(browser());

    SignInWithPrimaryAccount(browser()->profile());
    auto url = GURL(chrome::kChromeUISyncConfirmationURL);
    if (GetParam().sync_style == SyncConfirmationStyle::kSigninInterceptModal) {
      url = AppendSyncConfirmationQueryParams(url, GetParam().sync_style);
    }
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();

    auto* controller = browser()->signin_view_controller();
    controller->ShowModalSyncConfirmationDialog(
        GetParam().sync_style == SyncConfirmationStyle::kSigninInterceptModal);
    observer.Wait();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::SetUpCommandLine(GetParam(), scoped_env_override_, command_line);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> scoped_env_override_;
};

IN_PROC_BROWSER_TEST_P(SyncConfirmationUIDialogPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncConfirmationUIDialogPixelTest,
                         testing::ValuesIn(kDialogTestParams),
                         &ParamToTestSuffix);
