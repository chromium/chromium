// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/startup/first_run_test_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/profiles/profile_picker_interactive_uitest_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error "Unsupported platform"
#endif

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfilePickerViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonEnabled);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonDisabled);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kSignInButton{"intro-app", "sign-in-promo",
                              "#acceptSignInButton"};
const DeepQuery kDontSignInButton{"intro-app", "sign-in-promo",
                                  "#declineSignInButton"};
const DeepQuery kLegacyDeclineManagementButton{
    "legacy-managed-user-profile-notice-app", "#cancel-button"};
const DeepQuery kDeclineManagementButton{"managed-user-profile-notice-app",
                                         "#cancel-button"};
const DeepQuery kOptInSyncButton{"sync-confirmation-app", "#confirmButton"};
const DeepQuery kDontSyncButton{"sync-confirmation-app", "#notNowButton"};
const DeepQuery kSettingsButton{"sync-confirmation-app", "#settingsButton"};
const DeepQuery kConfirmDefaultBrowserButton{"default-browser-app",
                                             "#confirmButton"};
const DeepQuery kSearchEngineChoiceActionButton{"search-engine-choice-app",
                                                "#actionButton"};

enum class SyncButtonsFeatureConfig : int {
  // Deprecated: kDisabled = 0,
  // Simulate async load resulting in not-equal buttons.
  kAsyncNotEqualButtons = 1,
  // Simulate async load resulting in equal buttons.
  kAsyncEqualButtons = 2,
  // Simulate async load that will deadline.
  kDeadlined = 3,
  // User interacts with the UI before capabilities are loaded.
  kButtonsStillLoading = 4,
};

struct TestParam {
  std::string test_suffix;
  bool with_privacy_sandbox_enabled = false;
  SyncButtonsFeatureConfig sync_buttons_feature_config =
      SyncButtonsFeatureConfig::kAsyncNotEqualButtons;
  bool with_updated_profile_creation_screen = false;
};

// Returned type is optional, because for the kButtonsStillLoading no buttons
// are yet presented (consequently, no metric recorded).
std::optional<::signin_metrics::SyncButtonsType> ExpectedButtonShownMetric(
    SyncButtonsFeatureConfig config) {
  switch (config) {
    case SyncButtonsFeatureConfig::kAsyncNotEqualButtons:
      return ::signin_metrics::SyncButtonsType::kSyncNotEqualWeighted;
    case SyncButtonsFeatureConfig::kAsyncEqualButtons:
      return ::signin_metrics::SyncButtonsType::
          kSyncEqualWeightedFromCapability;
    case SyncButtonsFeatureConfig::kDeadlined:
      return ::signin_metrics::SyncButtonsType::kSyncEqualWeightedFromDeadline;
    default:
      return std::nullopt;
  }
}

::signin_metrics::SyncButtonClicked ExpectedOptInButtonClickedMetric(
    SyncButtonsFeatureConfig config) {
  switch (config) {
    case SyncButtonsFeatureConfig::kAsyncNotEqualButtons:
      return ::signin_metrics::SyncButtonClicked::kSyncOptInNotEqualWeighted;
    case SyncButtonsFeatureConfig::kAsyncEqualButtons:
    case SyncButtonsFeatureConfig::kDeadlined:
      return ::signin_metrics::SyncButtonClicked::kSyncOptInEqualWeighted;
    default:
      NOTREACHED();
  }
}

::signin_metrics::SyncButtonClicked ExpectedDeclinedButtonClickedMetric(
    SyncButtonsFeatureConfig config) {
  switch (config) {
    case SyncButtonsFeatureConfig::kAsyncNotEqualButtons:
      return ::signin_metrics::SyncButtonClicked::kSyncCancelNotEqualWeighted;
    case SyncButtonsFeatureConfig::kAsyncEqualButtons:
    case SyncButtonsFeatureConfig::kDeadlined:
      return ::signin_metrics::SyncButtonClicked::kSyncCancelEqualWeighted;
    default:
      NOTREACHED();
  }
}

::signin_metrics::SyncButtonClicked ExpectedSettingsButtonClickedMetric(
    SyncButtonsFeatureConfig config) {
  switch (config) {
    case SyncButtonsFeatureConfig::kAsyncNotEqualButtons:
      return ::signin_metrics::SyncButtonClicked::kSyncSettingsNotEqualWeighted;
    case SyncButtonsFeatureConfig::kAsyncEqualButtons:
    case SyncButtonsFeatureConfig::kDeadlined:
      return ::signin_metrics::SyncButtonClicked::kSyncSettingsEqualWeighted;
    case SyncButtonsFeatureConfig::kButtonsStillLoading:
      return ::signin_metrics::SyncButtonClicked::kSyncSettingsUnknownWeighted;
  }
}

std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

// Permutations of supported parameters.
const TestParam kTestParams[] = {
    {.test_suffix = "Default"},
    {.test_suffix = "WithUpdatedProfileCreationScreen",
     .with_updated_profile_creation_screen = true},
    {.test_suffix = "AsyncCapabilitiesToNotEqualButtons",
     .sync_buttons_feature_config =
         SyncButtonsFeatureConfig::kAsyncEqualButtons},
    {.test_suffix = "AsyncCapabilitiesToEqualButtons",
     .sync_buttons_feature_config =
         SyncButtonsFeatureConfig::kAsyncNotEqualButtons},
    {.test_suffix = "AsyncCapabilitiesDeadlined",
     .sync_buttons_feature_config = SyncButtonsFeatureConfig::kDeadlined},
    {.test_suffix = "AsyncCapabilitiesPending",
     .sync_buttons_feature_config =
         SyncButtonsFeatureConfig::kButtonsStillLoading},
    {.test_suffix = "WithPrivacySandboxEnabled",
     .with_privacy_sandbox_enabled = true},
};

}  // namespace

class FirstRunInteractiveUiTestBase
    : public InteractiveBrowserTestT<FirstRunServiceBrowserTestBase>,
      public WithProfilePickerInteractiveUiTestHelpers {
 public:
  FirstRunInteractiveUiTestBase() = default;
  ~FirstRunInteractiveUiTestBase() override = default;

 protected:
  const std::string kTestGivenName = "Joe";
  const std::string kTestEmail = "joe.consumer@gmail.com";
  const std::string kTestEnterpriseEmail = "joe.consumer@chromium.org";

  // FirstRunServiceBrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    FirstRunServiceBrowserTestBase::SetUpInProcessBrowserTestFixture();
    url_loader_factory_helper_.SetUp();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return url_loader_factory_helper_.test_url_loader_factory();
  }

  void OpenFirstRun(base::OnceCallback<void(bool)> first_run_exited_callback =
                        base::OnceCallback<void(bool)>()) {
    ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

    fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                        std::move(first_run_exited_callback));

    WaitForPickerWidgetCreated();
    view()->SetProperty(views::kElementIdentifierKey, kProfilePickerViewId);
  }

  StateChange IsVisible(const DeepQuery& where) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = where;
    state_change.event = kElementExistsEvent;
    // Also enforce that none of the parents have "display: none" (which is
    // the case for some intro containers during the initial animation):
    // https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/offsetParent
    state_change.test_function = "(el) => el.offsetParent !== null";
    return state_change;
  }

  auto WaitForPickerDeletion() {
    return Steps(
        WaitForHide(kProfilePickerViewId, /*transition_only_on_event=*/true),

        // Note: The widget/view is destroyed asynchronously, we need to flush
        // the message loops to be able to reliably check the global state.
        CheckResult(&ProfilePicker::IsOpen, testing::IsFalse()));
  }

  auto PressJsButton(const ui::ElementIdentifier web_contents_id,
                     const DeepQuery& button_query) {
    // This can close/navigate the current page, so don't wait for success.
    return ExecuteJsAt(web_contents_id, button_query, "(btn) => btn.click()",
                       ExecuteJsMode::kFireAndForget);
  }

  auto WaitForButtonEnabled(const ui::ElementIdentifier web_contents_id,
                            const DeepQuery& button_query) {
    StateChange button_enabled;
    button_enabled.event = kButtonEnabled;
    button_enabled.where = button_query;
    button_enabled.type = StateChange::Type::kExistsAndConditionTrue;
    button_enabled.test_function = "(btn) => !btn.disabled";
    return WaitForStateChange(web_contents_id, button_enabled);
  }

  auto WaitForButtonDisabled(const ui::ElementIdentifier web_contents_id,
                             const DeepQuery& button_query) {
    StateChange button_disabled;
    button_disabled.event = kButtonDisabled;
    button_disabled.where = button_query;
    button_disabled.type = StateChange::Type::kExistsAndConditionTrue;
    button_disabled.test_function = "(btn) => btn.disabled";
    return WaitForStateChange(web_contents_id, button_disabled);
  }

  auto WaitForButtonVisible(const ui::ElementIdentifier web_contents_id,
                            const DeepQuery& button_query) {
    StateChange button_disabled;
    button_disabled.event = kButtonDisabled;
    button_disabled.where = button_query;
    button_disabled.type = StateChange::Type::kExistsAndConditionTrue;
    // See
    // chrome/browser/resources/signin/sync_confirmation/sync_confirmation_app.ts::getConfirmButtonClass_
    // to understand how buttons are hidden for the duration of capability
    // loading.
    button_disabled.test_function =
        "(btn) => !btn.classList.contains('visibility-hidden')";
    return WaitForStateChange(web_contents_id, button_disabled);
  }

  // Waits for the intro buttons to be shown and presses to proceed according
  // to the value of `sign_in`.
  auto CompleteIntroStep(bool sign_in) {
    const DeepQuery& button = sign_in ? kSignInButton : kDontSignInButton;
    return Steps(
        WaitForWebContentsReady(kWebContentsId,
                                GURL(chrome::kChromeUIIntroURL)),

        // Waiting for the animation to complete so we can start interacting
        // with the button.
        WaitForStateChange(kWebContentsId, IsVisible(button)),

        // Advance to the sign-in page.
        // Note: the button should be disabled after this, but there is no good
        // way to verify it in this sequence. It is verified by unit tests in
        // chrome/test/data/webui/intro/sign_in_promo_test.ts
        PressJsButton(kWebContentsId, button));
  }
 private:
  ChromeSigninClientWithURLLoaderHelper url_loader_factory_helper_;
};

class FirstRunParameterizedInteractiveUiTest
    : public FirstRunInteractiveUiTestBase,
      public testing::WithParamInterface<TestParam> {
 public:
  FirstRunParameterizedInteractiveUiTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features_and_params;
    std::vector<base::test::FeatureRef> disabled_features;
    scoped_chrome_build_override_ = std::make_unique<base::AutoReset<bool>>(
        SearchEngineChoiceDialogServiceFactory::
            ScopedChromeBuildOverrideForTesting(
                /*force_chrome_build=*/true));

    if (WithUpdatedProfileCreationScreen()) {
      enabled_features_and_params.push_back(
          {features::kEnterpriseUpdatedProfileCreationScreen, {}});
    } else {
      disabled_features.push_back(
          features::kEnterpriseUpdatedProfileCreationScreen);
    }

    if (WithPrivacySandboxEnabled()) {
      enabled_features_and_params.push_back(
          {privacy_sandbox::kPrivacySandboxSettings4,
           {{privacy_sandbox::kPrivacySandboxSettings4ForceShowConsentForTesting
                 .name,
             "true"}}});
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features_and_params, disabled_features);
  }

  // FirstRunInteractiveUiTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FirstRunInteractiveUiTestBase::SetUpCommandLine(command_line);

    // Change the country to belgium so that the search engine choice test works
    // as intended.
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");

    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);

    // The default browser step is normally only shown on Windows. If it's
    // forced, it should be shown on the other platforms for testing.
    command_line->AppendSwitch(switches::kForceFreDefaultBrowserStep);
  }

  void SetUp() override {
    if (WithPrivacySandboxEnabled()) {
      ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    }
    FirstRunInteractiveUiTestBase::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    FirstRunInteractiveUiTestBase::SetUpInProcessBrowserTestFixture();
    if (WithPrivacySandboxEnabled()) {
      PrivacySandboxService::SetPromptDisabledForTests(false);
    }
  }

  void SetUpOnMainThread() override {
    FirstRunInteractiveUiTestBase::SetUpOnMainThread();

    if (WithPrivacySandboxEnabled()) {
      host_resolver()->AddRule("*", "127.0.0.1");
      embedded_test_server()->StartAcceptingConnections();
    }

    SearchEngineChoiceDialogService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);
  }

  static bool WithUpdatedProfileCreationScreen() {
    return GetParam().with_updated_profile_creation_screen;
  }

  static bool WithPrivacySandboxEnabled() {
    return GetParam().with_privacy_sandbox_enabled;
  }

  static enum SyncButtonsFeatureConfig SyncButtonsFeatureConfig() {
    return GetParam().sync_buttons_feature_config;
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  auto CompleteSearchEngineChoiceStep() {
    const DeepQuery first_search_engine = {"search-engine-choice-app",
                                           "cr-radio-button"};
    const DeepQuery searchEngineChoiceList{"search-engine-choice-app",
                                           "#choiceList"};
    return Steps(
        WaitForWebContentsNavigation(
            kWebContentsId, GURL(chrome::kChromeUISearchEngineChoiceURL)),
        Do([&] {
          histogram_tester().ExpectBucketCount(
              search_engines::kSearchEngineChoiceScreenEventsHistogram,
              search_engines::SearchEngineChoiceScreenEvents::
                  kFreChoiceScreenWasDisplayed,
              1);
          EXPECT_EQ(user_action_tester_.GetActionCount(
                        "SearchEngineChoiceScreenShown"),
                    1);
        }),
        // Click on "More" to scroll to the bottom of the search engine list.
        PressJsButton(kWebContentsId, kSearchEngineChoiceActionButton),
        // The button should become disabled because we didn't make a choice.
        WaitForButtonDisabled(kWebContentsId, kSearchEngineChoiceActionButton),
        PressJsButton(kWebContentsId, first_search_engine),
        WaitForButtonEnabled(kWebContentsId, kSearchEngineChoiceActionButton),
        PressJsButton(kWebContentsId, kSearchEngineChoiceActionButton));
  }

  auto CompleteDefaultBrowserStep() {
    return Steps(
        WaitForWebContentsNavigation(
            kWebContentsId, GURL(chrome::kChromeUIIntroDefaultBrowserURL)),
        EnsurePresent(kWebContentsId, kConfirmDefaultBrowserButton),
        PressJsButton(kWebContentsId, kConfirmDefaultBrowserButton));
  }

 protected:
  void SimulateSignIn(const std::string& account_email,
                      const std::string& account_given_name) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile());

    // Kombucha note: This function waits on a `base::RunLoop`.
    AccountInfo account_info = signin::MakeAccountAvailable(
        identity_manager,
        signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .WithCookie()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithAccessPoint(
                signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE)
            .Build(account_email));

    account_info =
        signin::WithGeneratedUserInfo(account_info, account_given_name);
    if (account_email == kTestEnterpriseEmail) {
      account_info.hosted_domain = "chromium.org";
    }

    // Controls behavior of sync buttons.
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    switch (SyncButtonsFeatureConfig()) {
      case SyncButtonsFeatureConfig::kAsyncNotEqualButtons:
        mutator
            .set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
                true);
        break;
      case SyncButtonsFeatureConfig::kAsyncEqualButtons:
        mutator
            .set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
                false);
        break;
      case SyncButtonsFeatureConfig::kDeadlined:
      case SyncButtonsFeatureConfig::kButtonsStillLoading:
        // Screen configures itself without capabilities.
        break;
    }

    ASSERT_TRUE(account_info.IsValid());

    // Kombucha note: This function waits on a `base::RunLoop`.
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    content::WebContents* picker_contents =
        ProfilePicker::GetWebViewForTesting()->GetWebContents();
    DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(picker_contents);
    CHECK(tab_helper);
    EXPECT_EQ(tab_helper->signin_access_point(),
              signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE);
    // Simulate the Dice "ENABLE_SYNC" header parameter.
    {
      auto process_dice_header_delegate_impl =
          ProcessDiceHeaderDelegateImpl::Create(web_contents());
      process_dice_header_delegate_impl->EnableSync(account_info);
    }
  }

 private:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::AutoReset<bool>> scoped_chrome_build_override_;
};

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunParameterizedInteractiveUiTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);

// This test doesn't check for the search engine choice and privacy sandbox
// dialogs because the point of the test suite is to check what's happening in
// the FRE and not after it is closed.
IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTest, CloseWindow) {
  base::test::TestFuture<bool> proceed_future;

  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      WaitForWebContentsReady(kWebContentsId, GURL(chrome::kChromeUIIntroURL)),

      // Send "Close window" keyboard shortcut and wait for view to close.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_CLOSE_WINDOW))
          .SetMustRemainVisible(false));
  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
  ASSERT_TRUE(IsProfileNameDefault());

  // Checking the expected metrics from this flow.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kQuitAtEnd, 1);
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTest,
                       CloseChromeWithKeyboardShortcut) {
  base::test::TestFuture<bool> proceed_future;

  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      WaitForWebContentsReady(kWebContentsId, GURL(chrome::kChromeUIIntroURL)),

      // Send "Close app" keyboard shortcut. Note that this may synchronously
      // close the dialog so we need to let the step know that this is ok.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_EXIT))
          .SetMustRemainVisible(false));

  WaitForPickerClosed();

  EXPECT_FALSE(proceed_future.Get());
  histogram_tester().ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kAbandonedFlow, 1);
}
#endif

// TODO(crbug.com/366082752): Re-enable this test
IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTest,
                       DISABLED_SignInAndSync) {
  if (SyncButtonsFeatureConfig() ==
      SyncButtonsFeatureConfig::kButtonsStillLoading) {
    GTEST_SKIP() << "Sync not possible until buttons stop loading";
  }

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_64_BITS)
  // TODO(crbug.com/363254870): Re-enable this test
  if (SyncButtonsFeatureConfig() ==
      SyncButtonsFeatureConfig::kAsyncEqualButtons) {
    GTEST_SKIP() << "Test is flaky on win64";
  }
#endif  // WIN && ARCH_CPU_64_BITS

  base::test::TestFuture<bool> proceed_future;

  ASSERT_TRUE(IsProfileNameDefault());

  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      WaitForWebContentsReady(kWebContentsId, GURL(chrome::kChromeUIIntroURL)),

      // Waiting for the animation to complete so we can start interacting with
      // the button.
      WaitForStateChange(kWebContentsId, IsVisible(kSignInButton)),

      Do([&] {
        EXPECT_FALSE(GetFirstRunFinishedPrefValue());
        histogram_tester().ExpectUniqueSample(
            "Signin.SignIn.Offered",
            signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
      }),

      // Advance to the sign-in page.
      // Note: the button should be disabled after this, but there is no good
      // way to verify it in this sequence. It is verified by unit tests in
      // chrome/test/data/webui/intro/sign_in_promo_test.ts
      PressJsButton(kWebContentsId, kSignInButton),
      // Wait for switch to the Gaia sign-in page to complete.
      // Note: kPickerWebContentsId now points to the new profile's WebContents.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()),

      Do([&] {
        histogram_tester().ExpectUniqueSample(
            "Signin.SignIn.Started",
            signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
      }));

  // Pulled out of the test sequence because it waits using `RunLoop`s.
  SimulateSignIn(kTestEmail, kTestGivenName);

  GURL sync_page_url = AppendSyncConfirmationQueryParams(
      GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow,
      /*is_sync_promo=*/true);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  const DeepQuery first_search_engine = {"search-engine-choice-app",
                                         "cr-radio-button"};

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      // Web Contents already instrumented in the previous sequence.
      WaitForWebContentsNavigation(kWebContentsId, sync_page_url),

      Do([&] {
        histogram_tester().ExpectUniqueSample(
            "Signin.SyncOptIn.Started",
            signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
      }),

      // Button is visible once capabilities are loaded or defaulted.
      WaitForButtonVisible(kWebContentsId, kOptInSyncButton),

      EnsurePresent(kWebContentsId, kOptInSyncButton),
      PressJsButton(kWebContentsId, kOptInSyncButton)
          .SetMustRemainVisible(false),

      CompleteSearchEngineChoiceStep(), CompleteDefaultBrowserStep());

  WaitForPickerClosed();

  if (WithPrivacySandboxEnabled()) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabPageURL),
        WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    // Test that the Privacy Sandbox prompt gets displayed in the browser after
    // the user makes a Search Engine Choice in the FRE.
    PrivacySandboxService* privacy_sandbox_service =
        PrivacySandboxServiceFactory::GetForProfile(profile());
    EXPECT_TRUE(privacy_sandbox_service->IsPromptOpenForBrowser(browser()));
  }

  histogram_tester().ExpectUniqueSample(
      "Signin.SyncOptIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  histogram_tester().ExpectBucketCount("ProfilePicker.FirstRun.DefaultBrowser",
                                       DefaultBrowserChoice::kClickSetAsDefault,
                                       1);

  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet, 1);

  EXPECT_TRUE(proceed_future.Get());

  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), GetProfileName());
  EXPECT_FALSE(IsUsingDefaultProfileName());

  // Re-assessment of all metrics from this flow, and check for no
  // double-logs.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncOptIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncOptIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      *ExpectedButtonShownMetric(SyncButtonsFeatureConfig()), 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncButtons.Clicked",
      ExpectedOptInButtonClickedMetric(SyncButtonsFeatureConfig()), 1);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}

// TODO(crbug.com/366082752): Re-enable this test
IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTest,
                       DISABLED_DeclineSync) {
  if (SyncButtonsFeatureConfig() ==
      SyncButtonsFeatureConfig::kButtonsStillLoading) {
    GTEST_SKIP() << "Decline is not possible until buttons stop loading";
  }

  base::test::TestFuture<bool> proceed_future;

  ASSERT_TRUE(IsProfileNameDefault());

  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      // Wait for switch to the Gaia sign-in page to complete.
      // Note: kPickerWebContentsId now points to the new profile's WebContents.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  // Pulled out of the test sequence because it waits using `RunLoop`s.
  SimulateSignIn(kTestEmail, kTestGivenName);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForWebContentsNavigation(
          kWebContentsId,
          AppendSyncConfirmationQueryParams(GURL("chrome://sync-confirmation/"),
                                            SyncConfirmationStyle::kWindow,
                                            /*is_sync_promo=*/true)),

      // Button is visible once capabilities are loaded or defaulted.
      WaitForButtonVisible(kWebContentsId, kDontSyncButton),

      EnsurePresent(kWebContentsId, kDontSyncButton),
      PressJsButton(kWebContentsId, kDontSyncButton),

      CompleteSearchEngineChoiceStep(), CompleteDefaultBrowserStep());

  // Wait for the picker to be closed and deleted.
  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());

  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), GetProfileName());

  // Checking the expected metrics from this flow.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncOptIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectTotalCount("Signin.SyncOptIn.Completed", 0);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      *ExpectedButtonShownMetric(SyncButtonsFeatureConfig()), 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncButtons.Clicked",
      ExpectedDeclinedButtonClickedMetric(SyncButtonsFeatureConfig()), 1);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}

IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTest, GoToSettings) {
  base::test::TestFuture<bool> proceed_future;

  ASSERT_TRUE(IsProfileNameDefault());

  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      // Wait for switch to the Gaia sign-in page to complete.
      // Note: kPickerWebContentsId now points to the new profile's WebContents.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  // Pulled out of the test sequence because it waits using `RunLoop`s.
  SimulateSignIn(kTestEmail, kTestGivenName);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForWebContentsNavigation(
          kWebContentsId,
          AppendSyncConfirmationQueryParams(GURL("chrome://sync-confirmation/"),
                                            SyncConfirmationStyle::kWindow,
                                            /*is_sync_promo=*/true)),

      // Wait for opt-in button to appear for all test cases except for
      // kButtonsStillLoadings.
      If(
          [&]() {
            return SyncButtonsFeatureConfig() !=
                   SyncButtonsFeatureConfig::kButtonsStillLoading;
          },
          /* then_steps= */ Steps(
              WaitForButtonVisible(kWebContentsId, kOptInSyncButton))),

      // Click "Settings" to proceed to the browser.
      EnsurePresent(kWebContentsId, kSettingsButton),
      PressJsButton(kWebContentsId, kSettingsButton));

  // Wait for the picker to be closed and deleted.
  WaitForPickerClosed();
  ASSERT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kSyncSetupSubPage));

  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());
  EXPECT_FALSE(
      search_engine_choice_dialog_service->IsShowingDialog(*browser()));

  EXPECT_TRUE(proceed_future.Get());
  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), GetProfileName());

  // Checking the expected metrics from this flow.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncOptIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  if (ExpectedButtonShownMetric(SyncButtonsFeatureConfig()).has_value()) {
    histogram_tester().ExpectUniqueSample(
        "Signin.SyncButtons.Shown",
        *ExpectedButtonShownMetric(SyncButtonsFeatureConfig()), 1);
  }
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncButtons.Clicked",
      ExpectedSettingsButtonClickedMetric(SyncButtonsFeatureConfig()), 1);

  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}

// TODO(crbug.com/366119368): Re-enable this test
#if BUILDFLAG(IS_WIN)
#define MAYBE_PeekAndDeclineSignIn DISABLED_PeekAndDeclineSignIn
#else
#define MAYBE_PeekAndDeclineSignIn PeekAndDeclineSignIn
#endif
IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTest,
                       MAYBE_PeekAndDeclineSignIn) {
  base::test::TestFuture<bool> proceed_future;

  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      // Wait for switch to the Gaia sign-in page to complete.
      // Note: kPickerWebContentsId now points to the new profile's
      // WebContents.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()),

      // Navigate back.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      WaitForWebContentsNavigation(kWebContentsId,
                                   GURL(chrome::kChromeUIIntroURL)),

      // The buttons should be enabled so we can interact with them.
      EnsurePresent(kWebContentsId, kDontSignInButton),
      CheckJsResultAt(kWebContentsId, kSignInButton, "(e) => !e.disabled"),
      CheckJsResultAt(kWebContentsId, kDontSignInButton, "(e) => !e.disabled"),
      PressJsButton(kWebContentsId, kDontSignInButton),

      CompleteSearchEngineChoiceStep(), CompleteDefaultBrowserStep());

  WaitForPickerClosed();
  EXPECT_TRUE(proceed_future.Get());

  ASSERT_TRUE(IsProfileNameDefault());

  // Checking the expected metrics from this flow.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}

// TODO(crbug.com/366119368): Re-enable this test
#if BUILDFLAG(IS_WIN)
#define MAYBE_DeclineProfileManagement DISABLED_DeclineProfileManagement
#else
#define MAYBE_DeclineProfileManagement DeclineProfileManagement
#endif
IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTest,
                       MAYBE_DeclineProfileManagement) {
  base::test::TestFuture<bool> proceed_future;

  policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(
                     &policy::FakeUserPolicySigninService::BuildForEnterprise));
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(IsProfileNameDefault());

  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),

      CompleteIntroStep(/*sign_in=*/true),

      // Wait for switch to the Gaia sign-in page to complete.
      // Note: kPickerWebContentsId now points to the new profile's WebContents.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  // Pulled out of the test sequence because it waits using `RunLoop`s.
  SimulateSignIn(kTestEnterpriseEmail, kTestGivenName);
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  auto& decline_button = WithUpdatedProfileCreationScreen()
                             ? kDeclineManagementButton
                             : kLegacyDeclineManagementButton;
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      // Initially the loading screen is shown.
      WaitForWebContentsNavigation(
          kWebContentsId,
          AppendSyncConfirmationQueryParams(
              GURL(chrome::kChromeUISyncConfirmationURL)
                  .Resolve(chrome::kChromeUISyncConfirmationLoadingPath),
              SyncConfirmationStyle::kWindow, /*is_sync_promo=*/true)),

      // The FakeUserPolicySigninService resolves, indicating the the account
      // is managed and requiring to show the enterprise management opt-in.
      WaitForWebContentsNavigation(
          kWebContentsId, GURL(chrome::kChromeUIManagedUserProfileNoticeUrl)),
      EnsurePresent(kWebContentsId, decline_button),
      PressJsButton(kWebContentsId, decline_button),

      CompleteSearchEngineChoiceStep(), CompleteDefaultBrowserStep());

  // Wait for the picker to be closed and deleted.
  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());

  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_EQ(u"Person 1", GetProfileName());
  EXPECT_TRUE(IsUsingDefaultProfileName());

  // Checking the expected metrics from this flow.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncOptIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester().ExpectTotalCount("Signin.SyncOptIn.Completed", 0);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}
