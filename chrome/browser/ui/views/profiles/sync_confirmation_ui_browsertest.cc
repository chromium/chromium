// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/any_widget_observer.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT) && !BUILDFLAG(IS_CHROMEOS_LACROS)
#error Platform not supported
#endif

// TODO(crbug.com/1374702): Move this file next to sync_confirmation_ui.cc.
// Render the page in a browser instead of a profile_picker_view to be able to
// do so.

// Tests for the chrome://sync-confirmation WebUI page. They live here and not
// in the webui directory because they manipulate views.
namespace {
struct SyncConfirmationTestParam {
  PixelTestParam pixel_test_param;
  AccountManagementStatus account_management_status =
      AccountManagementStatus::kNonManaged;
  SyncConfirmationStyle sync_style = SyncConfirmationStyle::kWindow;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<SyncConfirmationTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const SyncConfirmationTestParam kWindowTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"}},
    {.pixel_test_param = {.test_suffix = "DarkTheme", .use_dark_theme = true}},
    {.pixel_test_param = {.test_suffix = "Rtl",
                          .use_right_to_left_language = true}},
    {.pixel_test_param = {.test_suffix = "SmallWindow",
                          .use_small_window = true}},
    {.pixel_test_param = {.test_suffix = "ManagedAccount"},
     .account_management_status = AccountManagementStatus::kManaged},
    {.pixel_test_param = {.test_suffix = "CR2023",
                          .use_chrome_refresh_2023_style = true}},
};

const SyncConfirmationTestParam kDialogTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
// The sign-in intercept feature isn't enabled on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    {.pixel_test_param = {.test_suffix = "SigninInterceptStyle"},
     .sync_style = SyncConfirmationStyle::kSigninInterceptModal},
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
    {.pixel_test_param = {.test_suffix = "DarkTheme", .use_dark_theme = true},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.pixel_test_param = {.test_suffix = "Rtl",
                          .use_right_to_left_language = true},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.pixel_test_param = {.test_suffix = "ManagedAccount"},
     .account_management_status = AccountManagementStatus::kManaged,
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.pixel_test_param = {.test_suffix = "CR2023",
                          .use_chrome_refresh_2023_style = true},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
};

GURL BuildSyncConfirmationWindowURL() {
  std::string url_string = chrome::kChromeUISyncConfirmationURL;
  return AppendSyncConfirmationQueryParams(GURL(url_string),
                                           SyncConfirmationStyle::kWindow);
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

  void OnNavigateBackRequested() override { NOTREACHED_NORETURN(); }

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
}  // namespace

class SyncConfirmationUIWindowPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<SyncConfirmationTestParam> {
 public:
  SyncConfirmationUIWindowPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {
    DCHECK(GetParam().sync_style == SyncConfirmationStyle::kWindow);
  }

  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    DCHECK(browser());

    SignInWithAccount(GetParam().account_management_status);
    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kPostSignInFlow,
        /*step_controller_factory=*/
        base::BindRepeating([](ProfilePickerWebContentsHost* host) {
          return std::unique_ptr<ProfileManagementStepController>(
              new SyncConfirmationStepControllerForTest(host));
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

    return VerifyPixelUi(widget, "SyncConfirmationUIWindowPixelTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ViewDeletedWaiter(profile_picker_view_).Wait();
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return profile_picker_view_->GetWidget();
  }

  raw_ptr<ProfileManagementStepTestView, DanglingUntriaged>
      profile_picker_view_;
};

IN_PROC_BROWSER_TEST_P(SyncConfirmationUIWindowPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncConfirmationUIWindowPixelTest,
                         testing::ValuesIn(kWindowTestParams),
                         &ParamToTestSuffix);

class SyncConfirmationUIDialogPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<SyncConfirmationTestParam> {
 public:
  SyncConfirmationUIDialogPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam().pixel_test_param) {
    DCHECK(GetParam().sync_style != SyncConfirmationStyle::kWindow);
  }

  ~SyncConfirmationUIDialogPixelTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    DCHECK(browser());

    SignInWithAccount(GetParam().account_management_status);
    auto url = GURL(chrome::kChromeUISyncConfirmationURL);
    if (GetParam().sync_style == SyncConfirmationStyle::kSigninInterceptModal) {
      url = AppendSyncConfirmationQueryParams(url, GetParam().sync_style);
    }
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();

    // ShowUi() can sometimes return before the dialog widget is shown because
    // the call to show the latter is asynchronous. Adding
    // NamedWidgetShownWaiter will prevent that from happening.
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");

    auto* controller = browser()->signin_view_controller();
    controller->ShowModalSyncConfirmationDialog(
        GetParam().sync_style == SyncConfirmationStyle::kSigninInterceptModal);
    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }
};

IN_PROC_BROWSER_TEST_P(SyncConfirmationUIDialogPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncConfirmationUIDialogPixelTest,
                         testing::ValuesIn(kDialogTestParams),
                         &ParamToTestSuffix);
