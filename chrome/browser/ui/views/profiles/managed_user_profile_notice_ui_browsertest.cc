// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/view_observer.h"

namespace {

struct ManagedUserProfileNoticePixelTestParam {
  PixelTestParam pixel_test_param;
  bool is_ui_refresh_enabled = false;
  bool use_primary_and_tonal_buttons = false;
};

std::string ParamToTestSuffix(
    const testing::TestParamInfo<ManagedUserProfileNoticePixelTestParam>&
        info) {
  return base::StrCat(
      {info.param.pixel_test_param.test_suffix,
       info.param.is_ui_refresh_enabled ? "Refresh" : "",
       info.param.use_primary_and_tonal_buttons ? "Tonal" : ""});
}

std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
CreateEnterpriseProfileCreationDialogParams(AccountInfo account_info) {
  return std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
      account_info,
      /*is_oidc_account=*/false,
      /*user_already_signed_in=*/false,
      /*profile_creation_required_by_policy=*/false,
      /*show_link_data_option=*/false,
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

        std::vector<ManagedUserProfileNoticePixelTestParam> params;
        for (const auto& window_param : kWindowTestParams) {
          for (bool is_ui_refresh_enabled : {false, true}) {
            for (bool use_primary_and_tonal_buttons : {false, true}) {
              params.push_back({.pixel_test_param = window_param,
                                .is_ui_refresh_enabled = is_ui_refresh_enabled,
                                .use_primary_and_tonal_buttons =
                                    use_primary_and_tonal_buttons});
            }
          }
        }
        return params;
      }());
  return *params;
}

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
    // Reload the WebUI in the picker contents.
    host()->ShowScreenInPickerContents(
        managed_user_notice_url_,
        base::BindOnce(&ManagedUserProfileNoticeStepControllerForTest::
                           OnManagedUserProfileNoticeLoaded,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(step_shown_callback)));
  }

  void OnNavigateBackRequested() override { NOTREACHED(); }

  void OnManagedUserProfileNoticeLoaded(
      StepSwitchFinishedCallback step_shown_callback) {
    ManagedUserProfileNoticeUI* managed_user_notice_ui =
        host()
            ->GetPickerContents()
            ->GetWebUI()
            ->GetController()
            ->GetAs<ManagedUserProfileNoticeUI>();

    CHECK(managed_user_notice_ui);
    managed_user_notice_ui->Initialize(
        /*browser=*/nullptr,
        ManagedUserProfileNoticeUI::ScreenType::kProfilePicker,
        CreateEnterpriseProfileCreationDialogParams(account_info_));

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
        {{switches::kFirstRunDesktopRefresh, GetParam().is_ui_refresh_enabled},
         {switches::kUsePrimaryAndTonalButtonsForPromos,
          GetParam().use_primary_and_tonal_buttons}});
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
    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
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
            GetParam().is_ui_refresh_enabled, account_info));
    profile_picker_view_->views::View::AddObserver(this);
    profile_picker_view_->ShowAndWait(GetParam().pixel_test_param.window_size);
    if (ProfilePicker::GetWebViewForTesting()) {
      profiles::testing::WaitForPickerUrl(
          GetParam().is_ui_refresh_enabled
              ? GURL(chrome::kChromeUIManagedUserProfileNoticeRefreshURL)
              : GURL(chrome::kChromeUIManagedUserProfileNoticeUrl));
    }
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    const testing::TestInfo* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

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
