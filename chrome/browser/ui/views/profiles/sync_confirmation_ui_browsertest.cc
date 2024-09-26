// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"

#include "base/functional/bind_internal.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_environment_variable_override.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/any_widget_observer.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT) && !BUILDFLAG(IS_CHROMEOS_LACROS)
#error Platform not supported
#endif

// TODO(crbug.com/40242558): Move this file next to sync_confirmation_ui.cc.
// Render the page in a browser instead of a profile_picker_view to be able to
// do so.

// Tests for the chrome://sync-confirmation WebUI page. They live here and not
// in the webui directory because they manipulate views.
namespace {

using testing::AllOf;
using testing::Contains;
using testing::ElementsAre;

// Configures the can_show_history_sync_opt_ins_without_minor_mode_restrictions
// account capability, which determines minor mode restrictions status.
using MinorModeRestrictions =
    base::StrongAlias<class MinorModeRestrictionsTag, signin::Tribool>;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
constexpr MinorModeRestrictions kWithUnrestrictedUser(signin::Tribool::kTrue);
constexpr MinorModeRestrictions kWithRestrictedUser(signin::Tribool::kFalse);
#endif

struct SyncConfirmationTestParam {
  PixelTestParam pixel_test_param;
  AccountManagementStatus account_management_status =
      AccountManagementStatus::kNonManaged;
  SyncConfirmationStyle sync_style = SyncConfirmationStyle::kWindow;
  bool is_sync_promo = false;
  MinorModeRestrictions minor_mode_restrictions = kWithUnrestrictedUser;
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

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    // Restricted mode is only implemented for these platforms.
    {.pixel_test_param = {.test_suffix =
                              "RegularWithRestrictionsWithUnrestrictedUser"},
     .minor_mode_restrictions = kWithUnrestrictedUser},
    {.pixel_test_param = {.test_suffix =
                              "RegularWithRestrictionsWithRestrictedUser"},
     .minor_mode_restrictions = kWithRestrictedUser},
#endif

};

const SyncConfirmationTestParam kDialogTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
// The sign-in intercept feature isn't enabled on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    {.pixel_test_param = {.test_suffix = "SigninInterceptStyle"},
     .sync_style = SyncConfirmationStyle::kSigninInterceptModal,
     .is_sync_promo = true},
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
    {.pixel_test_param = {.test_suffix = "DarkTheme", .use_dark_theme = true},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.pixel_test_param = {.test_suffix = "Rtl",
                          .use_right_to_left_language = true},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.pixel_test_param = {.test_suffix = "Promo"},
     .sync_style = SyncConfirmationStyle::kDefaultModal,
     .is_sync_promo = true},
    {.pixel_test_param = {.test_suffix = "ManagedAccount"},
     .account_management_status = AccountManagementStatus::kManaged,
     .sync_style = SyncConfirmationStyle::kDefaultModal},

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    // Restricted mode is only implemented for these platforms.
    {.pixel_test_param = {.test_suffix =
                              "RegularWithRestrictionsWithUnrestrictedUser"},
     .sync_style = SyncConfirmationStyle::kDefaultModal,
     .minor_mode_restrictions = kWithUnrestrictedUser},
    {.pixel_test_param = {.test_suffix =
                              "RegularWithRestrictionsWithRestrictedUser"},
     .sync_style = SyncConfirmationStyle::kDefaultModal,
     .minor_mode_restrictions = kWithRestrictedUser},
#endif

};

GURL BuildSyncConfirmationWindowURL() {
  std::string url_string = chrome::kChromeUISyncConfirmationURL;
  return AppendSyncConfirmationQueryParams(GURL(url_string),
                                           SyncConfirmationStyle::kWindow,
                                           /*is_sync_promo=*/true);
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

    SignInWithAccount(GetParam().account_management_status,
                      signin::ConsentLevel::kSignin,
                      GetParam().minor_mode_restrictions.value());
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
            ? std::optional<gfx::Size>(gfx::Size(750, 590))
            : std::nullopt);
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

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

  base::test::ScopedFeatureList scoped_feature_list;
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

    SignInWithAccount(GetParam().account_management_status,
                      signin::ConsentLevel::kSignin,
                      GetParam().minor_mode_restrictions.value());
    auto url = GURL(chrome::kChromeUISyncConfirmationURL);
    url = AppendSyncConfirmationQueryParams(url, GetParam().sync_style,
                                            GetParam().is_sync_promo);
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
        GetParam().sync_style == SyncConfirmationStyle::kSigninInterceptModal,
        GetParam().is_sync_promo);
    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

IN_PROC_BROWSER_TEST_P(SyncConfirmationUIDialogPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncConfirmationUIDialogPixelTest,
                         testing::ValuesIn(kDialogTestParams),
                         &ParamToTestSuffix);

enum class SyncConfirmationUIAction { kTurnSyncOn, kGoToSettings };

class SyncConfirmationUITest
    : public SigninBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, SyncConfirmationUIAction, std::string>>,
      public LoginUIService::Observer {
 public:
  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    CHECK(GetProfile());
    // The test should close the sync confirmation dialog once the observer
    // method is called to simulate the real behavior more closely.
    login_ui_service_observation_.Observe(
        LoginUIServiceFactory::GetForProfile(GetProfile()));
  }

  void TearDownOnMainThread() override {
    // Stop observing the LoginUIService before destroying the profile.
    login_ui_service_observation_.Reset();
    SigninBrowserTestBase::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SigninBrowserTestBase::SetUpCommandLine(command_line);

    if (GetLanguage().empty()) {
      return;
    }
    command_line->AppendSwitchASCII(switches::kLang, GetLanguage());

    // On Linux & Lacros the command line switch has no effect, we need to use
    // environment variables to change the language.
    scoped_env_override_ =
        std::make_unique<base::ScopedEnvironmentVariableOverride>(
            "LANGUAGE", GetLanguage());
  }

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override {
    browser()->signin_view_controller()->CloseModalSignin();
  }

  [[nodiscard]] AccountInfo FillAccountInfoWithEscapedHtmlCharacters(
      const AccountInfo& account_info) {
    AccountInfo new_account_info = account_info;
    // The account name contains characters that are escaped in HTML.
    new_account_info.full_name = "The name's <>&\"', James\u00a0<>&\"'";
    new_account_info.given_name = new_account_info.full_name;
    //  Fill all required fields to make `AccountInfo` valid.
    new_account_info.hosted_domain = kNoHostedDomainFound;
    new_account_info.picture_url = "https://example.org/avatar";
    CHECK(new_account_info.IsValid());
    return new_account_info;
  }

  bool IsSigninIntercept() { return std::get<0>(GetParam()); }

  SyncConfirmationUIAction GetAction() { return std::get<1>(GetParam()); }

  std::string GetLanguage() { return std::get<2>(GetParam()); }

  consent_auditor::FakeConsentAuditor* consent_auditor() {
    return static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(GetProfile()));
  }

  int GetActionButtonLabelId() {
    switch (GetAction()) {
      case SyncConfirmationUIAction::kTurnSyncOn:
        return IsSigninIntercept()
                   ? IDS_SYNC_CONFIRMATION_TURN_ON_SYNC_BUTTON_LABEL
                   : IDS_SYNC_CONFIRMATION_CONFIRM_BUTTON_LABEL;
      case SyncConfirmationUIAction::kGoToSettings:
        return IDS_SYNC_CONFIRMATION_SETTINGS_BUTTON_LABEL;
    }
  }

  int GetTitleId() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    return IDS_SYNC_CONFIRMATION_TANGIBLE_SYNC_INFO_TITLE_LACROS;
#else
    return IsSigninIntercept()
               ? IDS_SYNC_CONFIRMATION_TANGIBLE_SYNC_INFO_TITLE_SIGNIN_INTERCEPT_V2
               : IDS_SYNC_CONFIRMATION_TANGIBLE_SYNC_INFO_TITLE;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  int GetDescriptionId() {
    return IDS_SYNC_CONFIRMATION_TANGIBLE_SYNC_INFO_DESC;
  }

 protected:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
    ConsentAuditorFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildFakeConsentAuditor));
  }

 private:
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      login_ui_service_observation_{this};
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> scoped_env_override_;
};

// Regression test for https://crbug.com/325749258.
IN_PROC_BROWSER_TEST_P(SyncConfirmationUITest,
                       RecordConsentWithEscapedHtmlCharacters) {
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  account_info = FillAccountInfoWithEscapedHtmlCharacters(account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  browser()->signin_view_controller()->ShowModalSyncConfirmationDialog(
      IsSigninIntercept(), /*is_sync_promo=*/true);
  switch (GetAction()) {
    case SyncConfirmationUIAction::kTurnSyncOn:
      EXPECT_TRUE(
          login_ui_test_utils::ConfirmSyncConfirmationDialog(browser()));
      break;
    case SyncConfirmationUIAction::kGoToSettings:
      EXPECT_TRUE(
          login_ui_test_utils::GoToSettingsSyncConfirmationDialog(browser()));
      break;
  }

  EXPECT_THAT(consent_auditor()->recorded_confirmation_ids(),
              ElementsAre(GetActionButtonLabelId()));
  EXPECT_THAT(
      consent_auditor()->recorded_id_vectors(),
      ElementsAre(AllOf(Contains(GetTitleId()), Contains(GetDescriptionId()))));
  EXPECT_THAT(consent_auditor()->recorded_features(),
              ElementsAre(consent_auditor::Feature::CHROME_SYNC));
}

std::string SyncConfirmationUITestParamToTestSuffix(
    const testing::TestParamInfo<SyncConfirmationUITest::ParamType>& info) {
  auto [is_signin_intercept, action, language] = info.param;
  return base::StrCat({language, is_signin_intercept ? "Intercept" : "",
                       action == SyncConfirmationUIAction::kTurnSyncOn
                           ? "Accept"
                           : "GoToSettings"});
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SyncConfirmationUITest,
    testing::Combine(
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        // Sign-in intercept is not supported on Lacros.
        testing::Values(false),
#else
        testing::Bool(),
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
        testing::Values(SyncConfirmationUIAction::kTurnSyncOn,
                        SyncConfirmationUIAction::kGoToSettings),
        testing::Values("", "pl")),
    &SyncConfirmationUITestParamToTestSuffix);
