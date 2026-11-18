// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/managed_user_profile_notice_test_utils.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

using ::managed_user_profile_notice_test::GetSelectors;
using ::managed_user_profile_notice_test::ManagedUserProfileNoticeSelectors;
using ::managed_user_profile_notice_test::WaitForAndClickProceedButton;
using ::managed_user_profile_notice_test::WaitForWebContentsLoaded;

enum class ScreenVersion {
  kOld,
  kRefreshed,
  kRevamped,
};

struct ManagedUserProfileNoticePixelTestParam {
  PixelTestParam pixel_test_param;
  ScreenVersion screen_version = ScreenVersion::kOld;
};

std::string ParamToTestSuffix(
    const testing::TestParamInfo<ManagedUserProfileNoticePixelTestParam>&
        info) {
  std::string screen_version_suffix;
  switch (info.param.screen_version) {
    case ScreenVersion::kOld:
      screen_version_suffix = "Old";
      break;
    case ScreenVersion::kRefreshed:
      screen_version_suffix = "Refreshed";
      break;
    case ScreenVersion::kRevamped:
      screen_version_suffix = "Revamped";
      break;
  }
  return base::StrCat(
      {info.param.pixel_test_param.test_suffix, screen_version_suffix});
}

std::vector<ManagedUserProfileNoticePixelTestParam> BuildTestParams(
    base::span<const PixelTestParam> pixel_test_params,
    base::span<const ScreenVersion> screen_versions) {
  std::vector<ManagedUserProfileNoticePixelTestParam> params;
  for (const PixelTestParam& pixel_test_param : pixel_test_params) {
    for (ScreenVersion screen_version : screen_versions) {
      params.push_back({.pixel_test_param = pixel_test_param,
                        .screen_version = screen_version});
    }
  }
  return params;
}

// Creates the params of the enterprise profile creation dialog. When
// `user_already_signed_in` is false the dialog starts at the value prop screen
// and otherwise at the disclosure screen.
// Proceeding from the disclosure screen shows the user data handling screen
// when `show_link_data_option` is true.
std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
CreateEnterpriseProfileCreationDialogParams(const AccountInfo& account_info,
                                            bool user_already_signed_in,
                                            bool show_link_data_option) {
  return std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
      account_info,
      /*is_oidc_account=*/false,
      /*user_already_signed_in=*/user_already_signed_in,
      /*profile_creation_required_by_policy=*/false,
      /*show_link_data_option=*/show_link_data_option,
      /*process_user_choice_callback=*/
      signin::SigninChoiceCallback(base::DoNothing()),
      /*done_callback=*/base::DoNothing());
}

const std::vector<ManagedUserProfileNoticePixelTestParam>& GetTestParams() {
  static const base::NoDestructor<
      std::vector<ManagedUserProfileNoticePixelTestParam>>
      params([] {
        const PixelTestParam kWindowTestParams[] = {
            {.test_suffix = "Regular"},
            {.test_suffix = "DarkTheme", .use_dark_theme = true},
            {.test_suffix = "Rtl", .use_right_to_left_language = true},
            {.test_suffix = "SmallWindow",
             .window_size = PixelTestParam::kSmallWindowSize},
        };
        static constexpr ScreenVersion kScreenVersions[] = {
            ScreenVersion::kOld, ScreenVersion::kRefreshed,
            ScreenVersion::kRevamped};
        return BuildTestParams(kWindowTestParams, kScreenVersions);
      }());
  return *params;
}

const std::vector<ManagedUserProfileNoticePixelTestParam>&
GetModalTestParams() {
  static const base::NoDestructor<
      std::vector<ManagedUserProfileNoticePixelTestParam>>
      params([] {
        const PixelTestParam kModalTestParams[] = {
            {.test_suffix = "Regular"},
            {.test_suffix = "DarkTheme", .use_dark_theme = true},
            {.test_suffix = "Rtl", .use_right_to_left_language = true},
        };
        static constexpr ScreenVersion kScreenVersions[] = {
            ScreenVersion::kOld, ScreenVersion::kRefreshed};
        return BuildTestParams(kModalTestParams, kScreenVersions);
      }());
  return *params;
}

// Parameters to set up the enterprise profile creation dialog on a given
// screen in `ManagedUserProfileNoticeUIModalPixelTest`.
struct ModalScreenParams {
  // Params of the dialog, see `CreateEnterpriseProfileCreationDialogParams()`.
  bool user_already_signed_in = false;
  bool show_link_data_option = false;
  // Whether to proceed from the initial screen to reach this screen.
  bool proceed_from_initial_screen = false;
  // Selector of the screen element in the app's shadow root.
  std::string_view selector;
};

// Creates a step to represent the managed-user-profile-notice.
class ManagedUserProfileNoticeStepControllerForTest
    : public ProfileManagementStepController {
 public:
  explicit ManagedUserProfileNoticeStepControllerForTest(
      ProfilePickerWebContentsHost* host,
      AccountInfo account_info,
      bool use_refreshed_ui)
      : ProfileManagementStepController(host),
        managed_user_notice_url_(
            use_refreshed_ui
                ? GURL(chrome::kChromeUIManagedUserProfileNoticeRefreshURL)
                : GURL(chrome::kChromeUIManagedUserProfileNoticeUrl)),
        account_info_(account_info) {}

  ~ManagedUserProfileNoticeStepControllerForTest() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    ManagedUserProfileNoticeParams::CreateForWebContents(
        host().GetPickerContents(),
        /*browser=*/nullptr,
        ManagedUserProfileNoticeUI::ScreenType::kProfilePicker,
        CreateEnterpriseProfileCreationDialogParams(
            account_info_,
            /*user_already_signed_in=*/false,
            /*show_link_data_option=*/false));

    // Reload the WebUI in the picker contents.
    host().ShowScreenInPickerContents(
        managed_user_notice_url_,
        base::BindOnce(&ManagedUserProfileNoticeStepControllerForTest::
                           OnManagedUserProfileNoticeLoaded,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(step_shown_callback)));
  }

  void OnManagedUserProfileNoticeLoaded(
      StepSwitchFinishedCallback step_shown_callback) {
    if (!step_shown_callback->is_null()) {
      std::move(step_shown_callback.value()).Run(/*success=*/true);
    }
  }

 private:
  const GURL managed_user_notice_url_;
  AccountInfo account_info_;
  base::WeakPtrFactory<ManagedUserProfileNoticeStepControllerForTest>
      weak_ptr_factory_{this};
};

}  // namespace

class ManagedUserProfileNoticeUIWindowPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<
          ManagedUserProfileNoticePixelTestParam>,
      public views::ViewObserver {
 public:
  ManagedUserProfileNoticeUIWindowPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {
    scoped_feature_list_.InitWithFeatureStates(
        {{switches::kFirstRunDesktopRefresh,
          GetParam().screen_version == ScreenVersion::kRefreshed ||
              GetParam().screen_version == ScreenVersion::kRevamped},
         {switches::kFirstRunDesktopRevamp,
          GetParam().screen_version == ScreenVersion::kRevamped}});
  }

  ~ManagedUserProfileNoticeUIWindowPixelTest() override {
    if (profile_picker_view_) {
      profile_picker_view_->views::View::RemoveObserver(this);
    }
  }

  void ShowUi(const std::string& name) override {
    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    CHECK(browser());

    AccountInfo account_info =
        SignInWithAccount(AccountManagementStatus::kManaged);
    const bool is_ui_refresh_enabled =
        GetParam().screen_version == ScreenVersion::kRefreshed ||
        GetParam().screen_version == ScreenVersion::kRevamped;
    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->GetProfile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kPostSignInFlow,
        /*step_controller_factory=*/
        base::BindRepeating(
            [](bool use_refreshed_ui, AccountInfo account_info,
               ProfilePickerWebContentsHost* host) {
              return std::unique_ptr<ProfileManagementStepController>(
                  std::make_unique<
                      ManagedUserProfileNoticeStepControllerForTest>(
                      host, account_info, use_refreshed_ui));
            },
            is_ui_refresh_enabled, account_info));
    profile_picker_view_->views::View::AddObserver(this);
    profile_picker_view_->ShowAndWait(GetParam().pixel_test_param.window_size);
    if (ProfilePicker::GetWebViewForTesting()) {
      profiles::testing::WaitForPickerUrl(
          is_ui_refresh_enabled
              ? GURL(chrome::kChromeUIManagedUserProfileNoticeRefreshURL)
              : GURL(chrome::kChromeUIManagedUserProfileNoticeUrl));
    }
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    const std::string baseline = "8481376";
    const testing::TestInfo* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name = base::StrCat(
        {test_info->test_suite_name(), "_", test_info->name(), "_", baseline});

    return VerifyPixelUi(widget, "ManagedUserProfileNoticeUIWindowPixelTest",
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

IN_PROC_BROWSER_TEST_P(ManagedUserProfileNoticeUIWindowPixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ManagedUserProfileNoticeUIWindowPixelTest,
                         testing::ValuesIn(GetTestParams()),
                         &ParamToTestSuffix);

class ManagedUserProfileNoticeUIModalPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<
          ManagedUserProfileNoticePixelTestParam> {
 public:
  ManagedUserProfileNoticeUIModalPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam().pixel_test_param) {
    scoped_feature_list_.InitWithFeatureStates(
        {{switches::kFirstRunDesktopRefresh, UseRefreshedUi()},
         {switches::kFirstRunDesktopChoiceScreenRefresh, UseRefreshedUi()},
         {switches::kEnterpriseSigninDialogsRefresh, UseRefreshedUi()}});
  }

  // Verifies the UI of the screen described by `params`.
  void ShowAndVerifyUi(const ModalScreenParams& params) {
    screen_params_ = params;
    DialogBrowserTest::ShowAndVerifyUi();
  }

  void ShowUi(const std::string& name) override {
    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    AccountInfo account_info =
        SignInWithAccount(AccountManagementStatus::kManaged);

    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");
    SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
        CreateEnterpriseProfileCreationDialogParams(
            account_info, screen_params_.user_already_signed_in,
            screen_params_.show_link_data_option));
    widget_waiter.WaitIfNeededAndGet();

    content::WebContents* web_contents =
        SigninViewController::From(browser())
            ->GetModalDialogWebContentsForTesting();
    const ManagedUserProfileNoticeSelectors& selectors =
        GetSelectors(UseRefreshedUi());
    if (screen_params_.proceed_from_initial_screen) {
      ASSERT_TRUE(WaitForAndClickProceedButton(web_contents, selectors));
    }
    ASSERT_TRUE(WaitForWebContentsLoaded(web_contents, selectors.app,
                                         screen_params_.selector));
  }

 private:
  bool UseRefreshedUi() const {
    return GetParam().screen_version == ScreenVersion::kRefreshed;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  ModalScreenParams screen_params_;
};

IN_PROC_BROWSER_TEST_P(ManagedUserProfileNoticeUIModalPixelTest,
                       InvokeUi_UserDataHandling) {
  ShowAndVerifyUi({.user_already_signed_in = true,
                   .show_link_data_option = true,
                   .proceed_from_initial_screen = true,
                   .selector = "#user-data-handling"});
}

IN_PROC_BROWSER_TEST_P(ManagedUserProfileNoticeUIModalPixelTest,
                       InvokeUi_ValueProp) {
  // The value proposition screen is the initial screen for a user that is not
  // signed in yet, hence most properties are set to false.
  ShowAndVerifyUi({.selector = "#value-prop"});
}

IN_PROC_BROWSER_TEST_P(ManagedUserProfileNoticeUIModalPixelTest,
                       InvokeUi_Disclosure) {
  ShowAndVerifyUi({.user_already_signed_in = true, .selector = "#disclosure"});
}

INSTANTIATE_TEST_SUITE_P(,
                         ManagedUserProfileNoticeUIModalPixelTest,
                         testing::ValuesIn(GetModalTestParams()),
                         &ParamToTestSuffix);
