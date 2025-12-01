// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/any_widget_observer.h"

// Tests for the chrome://history-sync-optin WebUI page.
namespace {

std::string ParamToTestSuffix(
    const testing::TestParamInfo<PixelTestParam>& info) {
  return info.param.test_suffix;
}

const PixelTestParam kDialogTestParams[] = {
    {.test_suffix = "Regular"},
    {.test_suffix = "DarkTheme", .use_dark_theme = true},
    {.test_suffix = "Rtl", .use_right_to_left_language = true},
};
}  // namespace

class HistorySyncOptinUIDialogPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  HistorySyncOptinUIDialogPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos},
        /*disabled_features=*/{});
  }

  ~HistorySyncOptinUIDialogPixelTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    CHECK(browser());

    SignInWithAccount();
    auto target_url = HistorySyncOptinUI::AppendHistorySyncOptinQueryParams(
        GURL(chrome::kChromeUIHistorySyncOptinURL),
        HistorySyncOptinLaunchContext::kModal);
    content::TestNavigationObserver observer(target_url);
    observer.StartWatchingNewWebContents();

    // ShowUi() can sometimes return before the dialog widget is shown because
    // the call to show the latter is asynchronous. Adding
    // NamedWidgetShownWaiter will prevent that from happening.
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");

    auto* controller = browser()->GetFeatures().signin_view_controller();
    controller->ShowModalHistorySyncOptInDialog(
        should_close_modal_dialog_,
        HistorySyncOptinHelper::FlowCompletedCallback(base::DoNothing()));
    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

 private:
  bool should_close_modal_dialog_ = true;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(HistorySyncOptinUIDialogPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         HistorySyncOptinUIDialogPixelTest,
                         testing::ValuesIn(kDialogTestParams),
                         &ParamToTestSuffix);

// Creates a step to represent the history-sync-optin.
class HistorySyncOptinStepControllerForTest
    : public ProfileManagementStepController {
 public:
  explicit HistorySyncOptinStepControllerForTest(
      ProfilePickerWebContentsHost* host)
      : ProfileManagementStepController(host),
        history_sync_optin_url_(
            HistorySyncOptinUI::AppendHistorySyncOptinQueryParams(
                GURL(chrome::kChromeUIHistorySyncOptinURL),
                HistorySyncOptinLaunchContext::kWindow)) {}

  ~HistorySyncOptinStepControllerForTest() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    // Reload the WebUI in the picker contents.
    host()->ShowScreenInPickerContents(
        history_sync_optin_url_,
        base::BindOnce(
            &HistorySyncOptinStepControllerForTest::OnHistorySyncOptinLoaded,
            weak_ptr_factory_.GetWeakPtr(), std::move(step_shown_callback)));
  }

  void OnNavigateBackRequested() override { NOTREACHED(); }

  void OnHistorySyncOptinLoaded(
      StepSwitchFinishedCallback step_shown_callback) {
    HistorySyncOptinUI* history_sync_optin_ui =
        static_cast<HistorySyncOptinUI*>(
            host()->GetPickerContents()->GetWebUI()->GetController());

    history_sync_optin_ui->Initialize(
        /*browser=*/nullptr,
        // Value does not matter when browser is null (window mode).
        /*should_close_modal_dialog=*/std::nullopt,
        HistorySyncOptinHelper::FlowCompletedCallback(base::DoNothing()));

    if (!step_shown_callback->is_null()) {
      std::move(step_shown_callback.value()).Run(/*success=*/true);
    }
  }

 private:
  const GURL history_sync_optin_url_;
  base::WeakPtrFactory<HistorySyncOptinStepControllerForTest> weak_ptr_factory_{
      this};
};

const PixelTestParam kWindowTestParams[] = {
    {.test_suffix = "Regular"},
    {.test_suffix = "DarkTheme", .use_dark_theme = true},
    {.test_suffix = "Rtl", .use_right_to_left_language = true},
    {.test_suffix = "SmallWindow",
     .window_size = PixelTestParam::kSmallWindowSize},
};

class HistorySyncOptinUIWindowPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<PixelTestParam>,
      public views::ViewObserver {
 public:
  HistorySyncOptinUIWindowPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos},
        /*disabled_features=*/{});
  }

  ~HistorySyncOptinUIWindowPixelTest() override {
    if (profile_picker_view_) {
      profile_picker_view_->views::View::RemoveObserver(this);
    }
  }

  void ShowUi(const std::string& name) override {
    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    CHECK(browser());

    SignInWithAccount();
    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kPostSignInFlow,
        /*step_controller_factory=*/
        base::BindRepeating([](ProfilePickerWebContentsHost* host) {
          return std::unique_ptr<ProfileManagementStepController>(
              new HistorySyncOptinStepControllerForTest(host));
        }));
    profile_picker_view_->views::View::AddObserver(this);
    profile_picker_view_->ShowAndWait(GetParam().window_size);
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "HistorySyncOptinUIWindowPixelTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    if (!profile_picker_view_) {
      return;
    }
    CHECK(GetWidgetForScreenshot());
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
};

IN_PROC_BROWSER_TEST_P(HistorySyncOptinUIWindowPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         HistorySyncOptinUIWindowPixelTest,
                         testing::ValuesIn(kWindowTestParams),
                         &ParamToTestSuffix);
