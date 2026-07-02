// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <variant>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/startup/first_run_test_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/first_run_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_interactive_uitest_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_toolbar.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/regional_capabilities/enums.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Bool;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

using Step = ::ProfileManagementFlowController::Step;
using DeepQuery = ::WebContentsInteractionTestUtil::DeepQuery;

class MockSoundsManager : public audio::SoundsManager {
 public:
  MockSoundsManager() = default;
  ~MockSoundsManager() override = default;

  MOCK_METHOD(
      bool,
      Initialize,
      (SoundKey key, int resource_id, media::AudioCodec codec, bool loop),
      (override));
  MOCK_METHOD(bool, Play, (SoundKey key), (override));
  MOCK_METHOD(bool, Stop, (SoundKey key), (override));
  MOCK_METHOD(bool, Pause, (SoundKey key), (override));
  MOCK_METHOD(base::TimeDelta, GetDuration, (SoundKey key), (override));
};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfilePickerViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonEnabled);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonDisabled);

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

GURL GetHistorySyncOptinURL() {
  return GURL("chrome://history-sync-optin?launch_context=0");
}

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

struct FirstRunVersion {
  struct Legacy {};

  struct Refreshed {
    switches::FirstRunDesktopSignInPromoVariation variant =
        switches::FirstRunDesktopSignInPromoVariation::kDefault;
  };

  struct Revamped {
    switches::FirstRunDesktopSignInPromoVariation variant =
        switches::FirstRunDesktopSignInPromoVariation::kDefault;
  };

  using Value = std::variant<Legacy, Refreshed, Revamped>;
};

struct TestParam {
  std::string test_suffix;
  SyncButtonsFeatureConfig sync_buttons_feature_config =
      SyncButtonsFeatureConfig::kAsyncNotEqualButtons;
  std::optional<bool> with_supervision;
  bool with_sync_engine_ready = true;
  FirstRunVersion::Value flow_version = FirstRunVersion::Legacy{};
};

std::string SupervisionToString(const TestParamInfo<TestParam>& info) {
  if (!info.param.with_supervision.has_value()) {
    return "";
  }
  return info.param.with_supervision.value() ? "ForSupervisedUser"
                                             : "ForAdultUser";
}

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

void ConfigureTestSyncService(
    syncer::SyncService* sync_service,
    syncer::SyncService::TransportState sync_transport_state) {
  auto* test_sync_service = static_cast<syncer::TestSyncService*>(sync_service);
  CHECK(test_sync_service);
  test_sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {});
  test_sync_service->SetMaxTransportState(sync_transport_state);
  test_sync_service->FireStateChanged();
}

std::string VersionSuffix(const FirstRunVersion::Value& version) {
  return std::visit(
      absl::Overload{
          [](FirstRunVersion::Legacy) { return "LegacyView"; },
          [](FirstRunVersion::Refreshed refreshed) {
            switch (refreshed.variant) {
              case switches::FirstRunDesktopSignInPromoVariation::kDefault:
                return "RefreshedViewDefault";
              case switches::FirstRunDesktopSignInPromoVariation::
                  kDontSignInInTheTopCorner:
                return "RefreshedViewDontSignInTopCorner";
              case switches::FirstRunDesktopSignInPromoVariation::
                  kDontSignInOnGaiaPage:
                return "RefreshedViewDontSignInGaiaPage";
            }
          },
          [](FirstRunVersion::Revamped revamped) {
            switch (revamped.variant) {
              case switches::FirstRunDesktopSignInPromoVariation::kDefault:
                return "RevampedViewDefault";
              case switches::FirstRunDesktopSignInPromoVariation::
                  kDontSignInInTheTopCorner:
                return "RevampedViewDontSignInTopCorner";
              case switches::FirstRunDesktopSignInPromoVariation::
                  kDontSignInOnGaiaPage:
                return "RevampedViewDontSignInGaiaPage";
            }
          }},
      version);
  NOTREACHED();
}

std::string ParamToTestSuffix(const TestParamInfo<TestParam>& info) {
  return info.param.test_suffix + SupervisionToString(info) +
         VersionSuffix(info.param.flow_version);
}

// Gets permutations of supported parameters.
const std::vector<TestParam>& GetTestParams() {
  static const base::NoDestructor<std::vector<TestParam>> kTestParams([]() {
    const TestParam base_test_params[] = {
        {.test_suffix = "Default"},
        {.test_suffix = "Default", .with_supervision = true},
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
        {.test_suffix = "DefaultWithSyncEngineAwaiting",
         .with_sync_engine_ready = false},
    };

    // Triplicate each test param to cover the legacy, refreshed, and revamped
    // views.
    std::vector<TestParam> test_params;
    test_params.reserve(std::size(base_test_params) *
                        std::variant_size<FirstRunVersion::Value>());
    for (const auto& test_param : base_test_params) {
      test_params.push_back(test_param);

      TestParam test_param_refreshed = test_param;
      test_param_refreshed.flow_version = FirstRunVersion::Refreshed{
          .variant = switches::FirstRunDesktopSignInPromoVariation::kDefault};
      test_params.push_back(std::move(test_param_refreshed));

      TestParam test_param_revamped = test_param;
      test_param_revamped.flow_version = FirstRunVersion::Revamped{
          .variant = switches::FirstRunDesktopSignInPromoVariation::kDefault};
      test_params.push_back(std::move(test_param_revamped));
    }
    return test_params;
  }());
  return *kTestParams;
}

}  // namespace

// Test suite with default params, and with Search Engine Choice and Default
// Browser screens disabled.
class FirstRunInteractiveUiBaseTest
    : public InteractiveFeaturePromoTestMixin<FirstRunServiceBrowserTestBase>,
      public WithProfilePickerInteractiveUiTestHelpers {
 public:
  explicit FirstRunInteractiveUiBaseTest(
      const TestParam& params = TestParam(),
      const std::vector<base::test::FeatureRefAndParams>&
          fixture_enabled_features = {},
      const std::vector<base::test::FeatureRef>& fixture_disabled_features = {})
      : InteractiveFeaturePromoTestMixin<FirstRunServiceBrowserTestBase>(
            UseDefaultTrackerAllowingPromos(
                {feature_engagement::kIPHSupervisedUserProfileSigninFeature})),
        params_(params) {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        fixture_enabled_features;
    std::vector<base::test::FeatureRef> disabled_features =
        fixture_disabled_features;
    std::visit(
        absl::Overload{
            [&](FirstRunVersion::Legacy) {
              disabled_features.push_back(switches::kFirstRunDesktopRefresh);
              disabled_features.push_back(switches::kFirstRunDesktopRevamp);
            },
            [&](FirstRunVersion::Refreshed refreshed) {
              enabled_features.push_back(
                  {switches::kFirstRunDesktopRefresh,
                   {{switches::kFirstRunDesktopSignInPromoVariation.name,
                     switches::kFirstRunDesktopSignInPromoVariation.GetName(
                         refreshed.variant)}}});
              enabled_features.push_back(
                  {switches::kFirstRunDesktopChoiceScreenRefresh, {}});
              disabled_features.push_back(switches::kFirstRunDesktopRevamp);
            },
            [&](FirstRunVersion::Revamped revamped) {
              enabled_features.push_back(
                  {switches::kFirstRunDesktopRefresh,
                   {{switches::kFirstRunDesktopSignInPromoVariation.name,
                     switches::kFirstRunDesktopSignInPromoVariation.GetName(
                         revamped.variant)}}});
              enabled_features.push_back(
                  {switches::kFirstRunDesktopChoiceScreenRefresh, {}});
              enabled_features.push_back(
                  {switches::kFirstRunDesktopRevamp, {}});
            }},
        params_.flow_version);
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  ~FirstRunInteractiveUiBaseTest() override = default;

 protected:
  const std::string kTestGivenName = "Joe";
  const std::string kTestEmail = "joe.consumer@gmail.com";
  const std::string kTestEnterpriseEmail = "joe.consumer@chromium.org";

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  const DeepQuery& GetSignInButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kSignInButtonRefreshed(
          {"sign-in-promo-refresh", "#acceptSignInButton"});
      return *kSignInButtonRefreshed;
    } else {
      static const base::NoDestructor<DeepQuery> kSignInButton(
          {"intro-app", "sign-in-promo", "#acceptSignInButton"});
      return *kSignInButton;
    }
  }

  const DeepQuery& GetDontSignInButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kDontSignInButtonRefreshed(
          {"sign-in-promo-refresh", "#declineSignInButton"});
      return *kDontSignInButtonRefreshed;
    } else {
      static const base::NoDestructor<DeepQuery> kDontSignInButton(
          {"intro-app", "sign-in-promo", "#declineSignInButton"});
      return *kDontSignInButton;
    }
  }

  const DeepQuery& GetAcceptManagementButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"managed-user-profile-notice-app-refresh", "#proceedButton"});
      return *kQuery;
    } else {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"managed-user-profile-notice-app", "#proceed-button"});
      return *kQuery;
    }
  }

  const DeepQuery& GetDeclineManagementButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"managed-user-profile-notice-app-refresh", "#cancelButton"});
      return *kQuery;
    } else {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"managed-user-profile-notice-app", "#cancel-button"});
      return *kQuery;
    }
  }

  const DeepQuery& GetConfirmDefaultBrowserButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"default-browser-app-refresh", "#confirm-button"});
      return *kQuery;
    } else {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"default-browser-app", "#confirmButton"});
      return *kQuery;
    }
  }

  const DeepQuery& GetFinishOrContinueStartBrowsingButtonQuery() const {
    static const base::NoDestructor<DeepQuery> kQuery(
        {"finish-or-continue-app", "#startBrowsingButton"});
    return *kQuery;
  }

  const DeepQuery& GetFinishOrContinueEducationButtonQuery() const {
    static const base::NoDestructor<DeepQuery> kQuery(
        {"finish-or-continue-app", "#continueEducationButton"});
    return *kQuery;
  }

  const DeepQuery& GetOptInSyncButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"sync-confirmation-app-refresh", "#confirmButton"});
      return *kQuery;
    } else {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"sync-confirmation-app", "#confirmButton"});
      return *kQuery;
    }
  }

  const DeepQuery& GetDontSyncButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"sync-confirmation-app-refresh", "#notNowButton"});
      return *kQuery;
    } else {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"sync-confirmation-app", "#notNowButton"});
      return *kQuery;
    }
  }

  const DeepQuery& GetSettingsButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"sync-confirmation-app-refresh", "#settingsButton"});
      return *kQuery;
    } else {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"sync-confirmation-app", "#settingsButton"});
      return *kQuery;
    }
  }

  const DeepQuery& GetOptInSyncHistoryButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"history-sync-optin-app-refresh", "#acceptButton"});
      return *kQuery;
    } else {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"history-sync-optin-app", "#acceptButton"});
      return *kQuery;
    }
  }

  const DeepQuery& GetDontSyncHistoryButtonQuery() const {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"history-sync-optin-app-refresh", "#rejectButton"});
      return *kQuery;
    } else {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"history-sync-optin-app", "#rejectButton"});
      return *kQuery;
    }
  }

  const DeepQuery& GetSearchEngineChoiceActionButtonQuery() {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"search-engine-choice-app-refresh", "#actionButton"});
      return *kQuery;
    }
    static const base::NoDestructor<DeepQuery> kQuery(
        {"search-engine-choice-app", "#actionButton"});
    return *kQuery;
  }

  const DeepQuery& GetSearchEngineChoiceCrRadioButtonQuery() {
    if (UseRefreshedView()) {
      static const base::NoDestructor<DeepQuery> kQuery(
          {"search-engine-choice-app-refresh", "cr-radio-button"});
      return *kQuery;
    }

    static const base::NoDestructor<DeepQuery> kQuery(
        {"search-engine-choice-app", "cr-radio-button"});
    return *kQuery;
  }

  virtual std::vector<std::string> GetForcedFeatureShowcaseSteps() const {
    return {};
  }

  // FirstRunServiceBrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    FirstRunServiceBrowserTestBase::SetUpInProcessBrowserTestFixture();
    url_loader_factory_helper_.SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    FirstRunServiceBrowserTestBase::SetUpCommandLine(command_line);
    if (UseRevampedView()) {
      command_line->AppendSwitchASCII(
          switches::kForceFreFeatureShowcaseSteps,
          base::JoinString(GetForcedFeatureShowcaseSteps(), ","));
    }
  }

  void SetUpCommandLineForChoiceScreen(base::CommandLine* command_line) {
    // Change the country to belgium so that the search engine choice test works
    // as intended.
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, "BE");

    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return url_loader_factory_helper_.test_url_loader_factory();
  }

  void OpenFirstRun(base::OnceCallback<void(bool)> first_run_exited_callback =
                        base::OnceCallback<void(bool)>()) {
    ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

    fre_service()->OpenFirstRunIfNeeded(std::move(first_run_exited_callback));

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
    const DeepQuery& button =
        sign_in ? GetSignInButtonQuery() : GetDontSignInButtonQuery();
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

  void SimulateSignIn(const std::string& account_email,
                      const std::string& account_given_name,
                      bool with_extended_info = true) {
    enable_disclaimer_on_primary_account_change_resetter_ =
        enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
            profile());
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile());

    auto options_builder =
        signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .WithCookie()
            .WithAccessPoint(signin_metrics::AccessPoint::kForYouFre);
    // Note: the primary account is set later by the call to
    // `ProcessDiceHeaderDelegateImpl::CompleteChromeSignInAfterGaiaSignin`.
    // Kombucha note: This function waits on a `base::RunLoop`.
    AccountInfo account_info = signin::MakeAccountAvailable(
        identity_manager, options_builder.Build(account_email));

    if (with_extended_info) {
      account_info =
          signin::WithGeneratedUserInfo(account_info, account_given_name);
    }

    // Controls behavior of sync buttons and supervision.
    if (with_extended_info) {
      account_info =
          AccountInfo::Builder(account_info)
              .SetHostedDomain(account_email == kTestEnterpriseEmail
                                   ? "chromium.org"
                                   : signin::constants::kNoHostedDomainFound)
              .Build();
    }
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_is_subject_to_enterprise_features(account_email ==
                                                  kTestEnterpriseEmail);

    if (params_.with_supervision.has_value()) {
      mutator.set_is_subject_to_parental_controls(
          params_.with_supervision.value_or(false));
    }

    switch (params_.sync_buttons_feature_config) {
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

    if (with_extended_info) {
      ASSERT_TRUE(account_info.IsValid());
    }

    // Kombucha note: This function waits on a `base::RunLoop`.
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    content::WebContents* picker_contents =
        ProfilePicker::GetWebViewForTesting()->GetWebContents();
    DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(picker_contents);
    CHECK(tab_helper);
    EXPECT_EQ(tab_helper->signin_access_point(),
              signin_metrics::AccessPoint::kForYouFre);
    // Simulate the Dice "ENABLE_SYNC" header parameter.
    // This step also sets the primary account.
    {
      auto process_dice_header_delegate_impl =
          ProcessDiceHeaderDelegateImpl::Create(web_contents());
      process_dice_header_delegate_impl->CompleteChromeSignInAfterGaiaSignin(
          account_info);
    }
  }

  auto CompleteSearchEngineChoiceStep() {
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
        PressJsButton(kWebContentsId, GetSearchEngineChoiceActionButtonQuery()),
        // The button should become disabled because we didn't make a choice.
        WaitForButtonDisabled(kWebContentsId,
                              GetSearchEngineChoiceActionButtonQuery()),
        PressJsButton(kWebContentsId,
                      GetSearchEngineChoiceCrRadioButtonQuery()),
        WaitForButtonEnabled(kWebContentsId,
                             GetSearchEngineChoiceActionButtonQuery()),
        PressJsButton(kWebContentsId,
                      GetSearchEngineChoiceActionButtonQuery()));
  }

  void ExpectStepHistograms(Step step,
                            bool shown,
                            bool with_exit = false,
                            size_t count = 1) {
    SCOPED_TRACE("Checking Step #" + base::ToString(static_cast<int>(step)));

    histogram_tester().ExpectBucketCount("ProfilePicker.FREFlow.StepStart",
                                         step, count);
    histogram_tester().ExpectBucketCount("ProfilePicker.FREFlow.StepEnd", step,
                                         count);
    if (shown) {
      histogram_tester().ExpectBucketCount("ProfilePicker.FREFlow.StepShown",
                                           step, count);
    } else {
      histogram_tester().ExpectBucketCount("ProfilePicker.FREFlow.StepSkipped",
                                           step, count);
    }

    if (shown) {
      histogram_tester().ExpectTotalCount(
          base::StrCat({"ProfilePicker.FREFlow.StepShownDuration",
                        GetStepHistogramSuffixForTesting(step)}),
          count);
    }
    histogram_tester().ExpectTotalCount(
        base::StrCat({"ProfilePicker.FREFlow.StepTotalDuration",
                      GetStepHistogramSuffixForTesting(step)}),
        count);

    if (with_exit) {
      histogram_tester().ExpectTotalCount(
          "ProfilePicker.FREFlow.FlowTotalDuration", 1);
      histogram_tester().ExpectBucketCount(
          "ProfilePicker.FREFlow.FlowEndedAtStep", step, 1);
    }
  }

  bool UseRefreshedView() const {
    return !std::holds_alternative<FirstRunVersion::Legacy>(
        params_.flow_version);
  }

  bool UseRevampedView() const {
    return std::holds_alternative<FirstRunVersion::Revamped>(
        params_.flow_version);
  }

  auto CompleteFinishOrContinueStep(bool start_browsing = true) {
    const GURL finish_or_continue_url = net::AppendQueryParameter(
        GURL(chrome::kChromeUIIntroURL)
            .Resolve(chrome::kChromeUIIntroFinishOrContinueSubPage),
        "showcase", base::ToString(!GetForcedFeatureShowcaseSteps().empty()));
    const DeepQuery& button =
        start_browsing ? GetFinishOrContinueStartBrowsingButtonQuery()
                       : GetFinishOrContinueEducationButtonQuery();
    return Steps(
        WaitForWebContentsNavigation(kWebContentsId, finish_or_continue_url),
        EnsurePresent(kWebContentsId, button),
        PressJsButton(kWebContentsId, button));
  }

 private:
  TestParam params_;

  ChromeSigninClientWithURLLoaderHelper url_loader_factory_helper_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  base::ScopedClosureRunner
      enable_disclaimer_on_primary_account_change_resetter_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class FirstRunInteractiveUiTest
    : public WithParamInterface<FirstRunVersion::Value>,
      public FirstRunInteractiveUiBaseTest {
 public:
  explicit FirstRunInteractiveUiTest(
      const std::vector<base::test::FeatureRefAndParams>&
          fixture_enabled_features = {})
      : FirstRunInteractiveUiBaseTest(TestParam{.flow_version = GetParam()},
                                      fixture_enabled_features) {}
};

IN_PROC_BROWSER_TEST_P(FirstRunInteractiveUiTest, SignInError) {
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, "*@other.org");

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      // Wait for switch to the Gaia sign-in page to complete.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  // Pulled out of the test sequence because it waits using `RunLoop`s.
  SimulateSignIn(kTestEnterpriseEmail, kTestGivenName);

  // Wait for the picker to be closed and deleted.
  WaitForPickerClosed();
  EXPECT_TRUE(proceed_future.Get());
  // The error dialog is shown to the user.
  RunTestSequence(
      InAnyContext(WaitForShow(SigninViewController::kSigninErrorViewId)));
  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    histogram_tester().ExpectUniqueSample(
        "ProfilePicker.FREFlow.SignInError",
        SigninUIError::Type::kUsernameNotAllowedByPatternFromPrefs, 1);
  }
}

IN_PROC_BROWSER_TEST_P(FirstRunInteractiveUiTest, ExitAtSignIn) {
  ASSERT_TRUE(IsProfileNameDefault());

  base::test::TestFuture<bool> proceed_future;
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
                                   GetSigninChromeSyncDiceUrl()),

      // Send "Close window" keyboard shortcut and wait for view to close.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_CLOSE_WINDOW))
          .SetMustRemainVisible(false));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());

  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kQuitAtEnd, 1);

  ExpectStepHistograms(Step::kIntro, /*shown=*/true);
  ExpectStepHistograms(Step::kAccountSelection, /*shown=*/true,
                       /*with_exit=*/true);
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunInteractiveUiTest,
                         Values(FirstRunVersion::Legacy{},
                                FirstRunVersion::Refreshed{},
                                FirstRunVersion::Revamped{}),
                         [](const TestParamInfo<FirstRunVersion::Value>& info) {
                           return VersionSuffix(info.param);
                         });

template <typename T>
class WithTestSyncServiceMixin : public T {
 public:
  using T::T;

 protected:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    T::SetUpBrowserContextKeyedServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
  }
};

using FirstRunInteractiveUiTestWithSyncService =
    WithTestSyncServiceMixin<FirstRunInteractiveUiTest>;

// TODO(crbug.com/366119368): Re-enable this test
#if BUILDFLAG(IS_WIN)
#define MAYBE_SignIn DISABLED_SignIn
#else
#define MAYBE_SignIn SignIn
#endif
// Simplified version of the Signin flow in the FRE, without the Search Engine
// Choice and Default Browser screen showing. For the full flow, check
// `FirstRunParameterizedInteractiveUiTest_SignInAndSync` test below.
IN_PROC_BROWSER_TEST_P(FirstRunInteractiveUiTestWithSyncService, MAYBE_SignIn) {
  ASSERT_TRUE(IsProfileNameDefault());

  base::test::TestFuture<bool> proceed_future;
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

  ConfigureTestSyncService(SyncServiceFactory::GetForProfile(profile()),
                           syncer::SyncService::TransportState::ACTIVE);
  // Pulled out of the test sequence because it waits using `RunLoop`s.
  SimulateSignIn(kTestEmail, kTestGivenName);

  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    GURL history_page_url = GetHistorySyncOptinURL();
    RunTestSequenceInContext(
        views::ElementTrackerViews::GetContextForView(view()),
        // Web Contents already instrumented in the previous sequence.
        If([this]() { return UseRevampedView(); },
           Then(WaitForWebContentsNavigation(
               kWebContentsId,
               GURL(chrome::kChromeUIIntroURL)
                   .Resolve(chrome::kChromeUIIntroSignInCelebrationSubPage)))),
        WaitForWebContentsNavigation(kWebContentsId, history_page_url),
        // Button is visible once capabilities are loaded or defaulted.
        WaitForButtonVisible(kWebContentsId, GetDontSyncHistoryButtonQuery()),
        EnsurePresent(kWebContentsId, GetDontSyncHistoryButtonQuery()),
        PressJsButton(kWebContentsId, GetDontSyncHistoryButtonQuery())
            .SetMustRemainVisible(false));
  } else {
    GURL sync_page_url = AppendSyncConfirmationQueryParams(
        GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow,
        /*is_sync_promo=*/true);
    RunTestSequenceInContext(
        views::ElementTrackerViews::GetContextForView(view()),
        // Web Contents already instrumented in the previous sequence.
        If([this]() { return UseRevampedView(); },
           Then(WaitForWebContentsNavigation(
               kWebContentsId,
               GURL(chrome::kChromeUIIntroURL)
                   .Resolve(chrome::kChromeUIIntroSignInCelebrationSubPage)))),
        WaitForWebContentsNavigation(kWebContentsId, sync_page_url),
        // Button is visible once capabilities are loaded or defaulted.
        WaitForButtonVisible(kWebContentsId, GetDontSyncButtonQuery()),
        EnsurePresent(kWebContentsId, GetDontSyncButtonQuery()),
        PressJsButton(kWebContentsId, GetDontSyncButtonQuery())
            .SetMustRemainVisible(false));
  }

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      If([this]() { return UseRevampedView(); },
         Then(CompleteFinishOrContinueStep())));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());

  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), GetProfileName());
  EXPECT_FALSE(IsUsingDefaultProfileName());

  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);

  ExpectStepHistograms(Step::kIntro, /*shown=*/true);
  ExpectStepHistograms(Step::kAccountSelection, /*shown=*/true);
  ExpectStepHistograms(Step::kPostSignInFlow, /*shown=*/true);
  int expected_step_shown_duration_count = 4;
  int expected_step_total_duration_count = 6;
  if (UseRevampedView()) {
    ExpectStepHistograms(Step::kFinishOrContinue, /*shown=*/true);
    ExpectStepHistograms(Step::kFeatureShowcase, /*shown=*/false);
    ++expected_step_shown_duration_count;
    ++expected_step_total_duration_count;
  } else {
    ExpectStepHistograms(Step::kDefaultBrowser, /*shown=*/false);
  }
  ExpectStepHistograms(Step::kSearchEngineChoice, /*shown=*/false);
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepShownDuration",
                                      expected_step_shown_duration_count);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepTotalDuration",
                                      expected_step_total_duration_count);
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunInteractiveUiTestWithSyncService,
                         Values(FirstRunVersion::Legacy{},
                                FirstRunVersion::Refreshed{},
                                FirstRunVersion::Revamped{}),
                         [](const TestParamInfo<FirstRunVersion::Value>& info) {
                           return VersionSuffix(info.param);
                         });

class FirstRunParameterizedInteractiveUiTest
    : public FirstRunInteractiveUiBaseTest,
      public WithParamInterface<TestParam> {
 public:
  FirstRunParameterizedInteractiveUiTest()
      : FirstRunInteractiveUiBaseTest(
            GetParam(),
            /*fixture_enabled_features=*/{
                {feature_engagement::kIPHSupervisedUserProfileSigninFeature,
                 {}}}) {
    scoped_chrome_build_override_ = std::make_unique<base::AutoReset<bool>>(
        SearchEngineChoiceDialogServiceFactory::
            ScopedChromeBuildOverrideForTesting(
                /*force_chrome_build=*/true));
  }

  // FirstRunInteractiveUiTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FirstRunInteractiveUiBaseTest::SetUpCommandLine(command_line);

    SetUpCommandLineForChoiceScreen(command_line);

    // The default browser step is normally only shown on Windows. If it's
    // forced, it should be shown on the other platforms for testing.
    command_line->AppendSwitch(switches::kForceFreDefaultBrowserStep);
  }

  void SetUpOnMainThread() override {
    FirstRunInteractiveUiBaseTest::SetUpOnMainThread();

    SearchEngineChoiceDialogService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);
  }

  static enum SyncButtonsFeatureConfig SyncButtonsFeatureConfig() {
    return GetParam().sync_buttons_feature_config;
  }

  static bool WithSupervisedUser() {
    return GetParam().with_supervision.value_or(false);
  }

  auto CompleteDefaultBrowserStep() {
    return Steps(
        WaitForWebContentsNavigation(
            kWebContentsId, GURL(chrome::kChromeUIIntroDefaultBrowserURL)),
        EnsurePresent(kWebContentsId, GetConfirmDefaultBrowserButtonQuery()),
        PressJsButton(kWebContentsId, GetConfirmDefaultBrowserButtonQuery()));
  }

  // Custom url tracker. This is used for tracking navigation before proceeding
  // to the history sync optin screen. The navigation might go through a spinner
  // screen which is provided today by a different url from the `target_url`.
  // TODO(crbug.com/445926827): Once the spinners are incorporated in the
  // history sync dialog, we can use `WaitForWebContentsNavigation`.
  InteractiveBrowserTestApi::StateChange PageWithUrl(
      const std::string& target_url) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    InteractiveBrowserTestApi::StateChange expected_url_change;
    expected_url_change.type =
        InteractiveBrowserTestApi::StateChange::Type::kConditionTrue;
    expected_url_change.event = kStateChange;
    expected_url_change.test_function =
        "() => window.location.href === '" + target_url + "'";
    // Important for not stopping the tracking through redirections.
    expected_url_change.continue_across_navigation = true;
    return expected_url_change;
  }

 private:
  std::unique_ptr<base::AutoReset<bool>> scoped_chrome_build_override_;
};

// This test doesn't check for the search engine choice dialog because the point
// of the test suite is to check what's happening in the FRE and not after it is
// closed.
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
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kQuitAtEnd, 1);

  ExpectStepHistograms(Step::kIntro, /*shown=*/true, /*with_exit=*/true);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepShownDuration",
                                      1);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepTotalDuration",
                                      1);
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

  ExpectStepHistograms(Step::kIntro, /*shown=*/true, /*with_exit=*/true);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepShownDuration",
                                      1);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepTotalDuration",
                                      1);
}
#endif

IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTest, GoToSettings) {
  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    GTEST_SKIP() << "History optin screen does not have a settings button";
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
      If([this]() { return UseRevampedView(); },
         Then(WaitForWebContentsNavigation(
             kWebContentsId,
             GURL(chrome::kChromeUIIntroURL)
                 .Resolve(chrome::kChromeUIIntroSignInCelebrationSubPage)))),
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
          Then(
              WaitForButtonVisible(kWebContentsId, GetOptInSyncButtonQuery()))),

      // Click "Settings" to proceed to the browser.
      EnsurePresent(kWebContentsId, GetSettingsButtonQuery()),
      PressJsButton(kWebContentsId, GetSettingsButtonQuery()));

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
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncOptIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);

  if (ExpectedButtonShownMetric(SyncButtonsFeatureConfig()).has_value()) {
    histogram_tester().ExpectUniqueSample(
        "Signin.SyncButtons.Shown",
        *ExpectedButtonShownMetric(SyncButtonsFeatureConfig()), 1);
  }
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);

  ExpectStepHistograms(Step::kIntro, /*shown=*/true);
  ExpectStepHistograms(Step::kAccountSelection, /*shown=*/true);
  ExpectStepHistograms(Step::kPostSignInFlow, /*shown=*/true);
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepShownDuration",
                                      4);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepTotalDuration",
                                      4);
  // Those steps are not even attempted
  ExpectStepHistograms(Step::kSearchEngineChoice, /*shown=*/false,
                       /*with_exit=*/false, /*count=*/0);
  ExpectStepHistograms(Step::kDefaultBrowser, /*shown=*/false,
                       /*with_exit=*/false, /*count=*/0);
}

// TODO(crbug.com/366119368): Re-enable this test
// TODO(crbug.com/525637007): Test is flaky on Linux TSan bots.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
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
      EnsurePresent(kWebContentsId, GetDontSignInButtonQuery()),
      CheckJsResultAt(kWebContentsId, GetSignInButtonQuery(),
                      "(e) => !e.disabled"),
      CheckJsResultAt(kWebContentsId, GetDontSignInButtonQuery(),
                      "(e) => !e.disabled"),
      PressJsButton(kWebContentsId, GetDontSignInButtonQuery()),

      CompleteSearchEngineChoiceStep(),
      If([this]() { return !UseRevampedView(); },
         Then(CompleteDefaultBrowserStep())),
      If([this]() { return UseRevampedView(); },
         Then(CompleteFinishOrContinueStep())));

  WaitForPickerClosed();
  EXPECT_TRUE(proceed_future.Get());

  ASSERT_TRUE(IsProfileNameDefault());

  // Checking the expected metrics from this flow.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);

  // Navigation back to the Intro page makes the step be shown twice.
  ExpectStepHistograms(Step::kIntro, /*shown=*/true, /*with_exit=*/false,
                       /*count=*/2);
  ExpectStepHistograms(Step::kAccountSelection, /*shown=*/true);
  ExpectStepHistograms(Step::kSearchEngineChoice, /*shown=*/true);
  int expected_step_total_duration_count = 6;
  if (UseRevampedView()) {
    ExpectStepHistograms(Step::kFeatureShowcase, /*shown=*/false);
    ExpectStepHistograms(Step::kFinishOrContinue, /*shown=*/true);
    // Feature showcase is not shown, but registered hence the extra count.
    ++expected_step_total_duration_count;
  } else {
    ExpectStepHistograms(Step::kDefaultBrowser, /*shown=*/true);
  }
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepShownDuration",
                                      6);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepTotalDuration",
                                      expected_step_total_duration_count);
  // Sign in was never completed - step is not even attempted.
  ExpectStepHistograms(Step::kPostSignInFlow, /*shown=*/false,
                       /*with_exit=*/false, /*count=*/0);
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
  SimulateSignIn(kTestEnterpriseEmail, kTestGivenName,
                 /*with_extended_info=*/false);
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      // Initially the loading screen is shown.
      WaitForWebContentsNavigation(
          kWebContentsId,
          AppendSyncConfirmationQueryParams(
              GURL(chrome::kChromeUISyncConfirmationURL)
                  .Resolve(chrome::kChromeUISyncConfirmationLoadingPath),
              SyncConfirmationStyle::kWindow, /*is_sync_promo=*/true)));

  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  account_info = signin::WithGeneratedUserInfo(account_info, kTestGivenName);
  account_info = AccountInfo::Builder(account_info)
                     .SetHostedDomain("chromium.org")
                     .Build();
  // Pulled out of the test sequence because it waits using `RunLoop`s.
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      // The FakeUserPolicySigninService resolves, indicating the the account
      // is managed and requiring to show the enterprise management opt-in.
      WaitForWebContentsNavigation(
          kWebContentsId,
          UseRefreshedView()
              ? ManagedUserProfileNoticeUI::GetURLForType(
                    ManagedUserProfileNoticeUI::ScreenType::kFirstRun)
              : GURL(chrome::kChromeUIManagedUserProfileNoticeUrl)),
      EnsurePresent(kWebContentsId, GetDeclineManagementButtonQuery()),
      PressJsButton(kWebContentsId, GetDeclineManagementButtonQuery()),

      CompleteSearchEngineChoiceStep(),
      If([this]() { return !UseRevampedView(); },
         Then(CompleteDefaultBrowserStep())),
      If([this]() { return UseRevampedView(); },
         Then(CompleteFinishOrContinueStep())));

  // Wait for the picker to be closed and deleted.
  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());

  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile()));
  EXPECT_EQ(
      // "Your Chrome"
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PLACEHOLDER_PROFILE_NAME),
      GetProfileName());
  EXPECT_TRUE(IsUsingDefaultProfileName());

  // Checking the expected metrics from this flow.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kForYouFre, 1);
  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    histogram_tester().ExpectUniqueSample(
        "Signin.HistorySyncOptIn.Started",
        signin_metrics::AccessPoint::kForYouFre, 0);
    histogram_tester().ExpectTotalCount("Signin.HistorySyncOptIn.Completed", 0);
  } else {
    histogram_tester().ExpectUniqueSample(
        "Signin.SyncOptIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
    histogram_tester().ExpectTotalCount("Signin.SyncOptIn.Completed", 0);
  }
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);

  ExpectStepHistograms(Step::kIntro, /*shown=*/true);
  ExpectStepHistograms(Step::kAccountSelection, /*shown=*/true);
  ExpectStepHistograms(Step::kPostSignInFlow, /*shown=*/true);
  ExpectStepHistograms(Step::kSearchEngineChoice, /*shown=*/true);
  int expected_step_total_duration_count = 6;
  if (UseRevampedView()) {
    ExpectStepHistograms(Step::kFeatureShowcase, /*shown=*/false);
    ExpectStepHistograms(Step::kFinishOrContinue, /*shown=*/true);
    // Feature showcase is not shown, but registered hence the extra count.
    ++expected_step_total_duration_count;
  } else {
    ExpectStepHistograms(Step::kDefaultBrowser, /*shown=*/true);
  }
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepShownDuration",
                                      6);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepTotalDuration",
                                      expected_step_total_duration_count);
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunParameterizedInteractiveUiTest,
                         ValuesIn(GetTestParams()),
                         &ParamToTestSuffix);

using FirstRunParameterizedInteractiveUiTestWithSyncService =
    WithTestSyncServiceMixin<FirstRunParameterizedInteractiveUiTest>;

IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTestWithSyncService,
                       SignInAndSync) {
  bool should_skip_test = false;
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/363254870, crbug.com/366082752): Re-enable this test
  should_skip_test = true;
#endif  // WIN
  if (should_skip_test) {
    GTEST_SKIP() << "Test is flaky on win64";
  }

  if (SyncButtonsFeatureConfig() ==
      SyncButtonsFeatureConfig::kButtonsStillLoading) {
    GTEST_SKIP() << "Sync not possible until buttons stop loading";
  }

  auto iph_delay =
      AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
          base::Seconds(0));
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
      WaitForStateChange(kWebContentsId, IsVisible(GetSignInButtonQuery())),

      Do([&] {
        EXPECT_FALSE(GetFirstRunFinishedPrefValue());
        histogram_tester().ExpectUniqueSample(
            "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre,
            1);
      }),

      // Advance to the sign-in page.
      // Note: the button should be disabled after this, but there is no good
      // way to verify it in this sequence. It is verified by unit tests in
      // chrome/test/data/webui/intro/sign_in_promo_test.ts
      PressJsButton(kWebContentsId, GetSignInButtonQuery()),
      // Wait for switch to the Gaia sign-in page to complete.
      // Note: kPickerWebContentsId now points to the new profile's WebContents.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()),

      Do([&] {
        histogram_tester().ExpectUniqueSample(
            "Signin.SignIn.Started", signin_metrics::AccessPoint::kForYouFre,
            1);
      }));

  auto sync_transport_state =
      GetParam().with_sync_engine_ready
          ? syncer::SyncService::TransportState::ACTIVE
          : syncer::SyncService::TransportState::INITIALIZING;
  ConfigureTestSyncService(SyncServiceFactory::GetForProfile(profile()),
                           sync_transport_state);
  // Pulled out of the test sequence because it waits using `RunLoop`s.
  SimulateSignIn(kTestEmail, kTestGivenName);

  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kForYouFre, 1);

  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    GURL history_page_url = GetHistorySyncOptinURL();
    RunTestSequenceInContext(
        views::ElementTrackerViews::GetContextForView(view()),
        // Web Contents already instrumented in the previous sequence.
        WaitForStateChange(kWebContentsId,
                           PageWithUrl(history_page_url.spec())),
        Do([&] {
          histogram_tester().ExpectUniqueSample(
              "Signin.HistorySyncOptIn.Started",
              signin_metrics::AccessPoint::kForYouFre, 1);
        }),
        // Button is visible once capabilities are loaded or defaulted.
        WaitForButtonVisible(kWebContentsId, GetOptInSyncHistoryButtonQuery()),

        EnsurePresent(kWebContentsId, GetOptInSyncHistoryButtonQuery()),
        PressJsButton(kWebContentsId, GetOptInSyncHistoryButtonQuery())
            .SetMustRemainVisible(false),

        CompleteSearchEngineChoiceStep(),
        If([this]() { return !UseRevampedView(); },
           Then(CompleteDefaultBrowserStep())),
        If([this]() { return UseRevampedView(); },
           Then(CompleteFinishOrContinueStep())));
  } else {
    GURL sync_page_url = AppendSyncConfirmationQueryParams(
        GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow,
        /*is_sync_promo=*/true);
    RunTestSequenceInContext(
        views::ElementTrackerViews::GetContextForView(view()),
        // Web Contents already instrumented in the previous sequence.
        WaitForStateChange(kWebContentsId, PageWithUrl(sync_page_url.spec())),
        Do([&] {
          histogram_tester().ExpectUniqueSample(
              "Signin.SyncOptIn.Started",
              signin_metrics::AccessPoint::kForYouFre, 1);
        }),

        // Button is visible once capabilities are loaded or defaulted.
        WaitForButtonVisible(kWebContentsId, GetOptInSyncButtonQuery()),

        EnsurePresent(kWebContentsId, GetOptInSyncButtonQuery()),
        PressJsButton(kWebContentsId, GetOptInSyncButtonQuery())
            .SetMustRemainVisible(false),

        CompleteSearchEngineChoiceStep(),
        If([this]() { return !UseRevampedView(); },
           Then(CompleteDefaultBrowserStep())),
        If([this]() { return UseRevampedView(); },
           Then(CompleteFinishOrContinueStep())));
  }

  WaitForPickerClosed();

  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    histogram_tester().ExpectUniqueSample(
        "Signin.HistorySyncOptIn.Completed",
        signin_metrics::AccessPoint::kForYouFre, 1);
  } else {
    histogram_tester().ExpectUniqueSample(
        "Signin.SyncOptIn.Completed", signin_metrics::AccessPoint::kForYouFre,
        1);
  }

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
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kForYouFre, 1);
  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    histogram_tester().ExpectUniqueSample(
        "Signin.HistorySyncOptIn.Started",
        signin_metrics::AccessPoint::kForYouFre, 1);
    histogram_tester().ExpectUniqueSample(
        "Signin.HistorySyncOptIn.Completed",
        signin_metrics::AccessPoint::kForYouFre, 1);
  } else {
    histogram_tester().ExpectUniqueSample(
        "Signin.SyncOptIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
    histogram_tester().ExpectUniqueSample(
        "Signin.SyncOptIn.Completed", signin_metrics::AccessPoint::kForYouFre,
        1);
  }
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      *ExpectedButtonShownMetric(SyncButtonsFeatureConfig()), 1);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);

  ExpectStepHistograms(Step::kIntro, /*shown=*/true);
  ExpectStepHistograms(Step::kAccountSelection, /*shown=*/true);
  ExpectStepHistograms(Step::kPostSignInFlow, /*shown=*/true);
  ExpectStepHistograms(Step::kSearchEngineChoice, /*shown=*/true);
  int expected_step_total_duration_count = 6;
  if (UseRevampedView()) {
    ExpectStepHistograms(Step::kFeatureShowcase, /*shown=*/false);
    ExpectStepHistograms(Step::kFinishOrContinue, /*shown=*/true);
    // Feature showcase is not shown, but registered hence the extra count.
    ++expected_step_total_duration_count;
  } else {
    ExpectStepHistograms(Step::kDefaultBrowser, /*shown=*/true);
    histogram_tester().ExpectBucketCount(
        "ProfilePicker.FirstRun.DefaultBrowser",
        DefaultBrowserChoice::kClickSetAsDefault, 1);
  }
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepShownDuration",
                                      6);
  histogram_tester().ExpectTotalCount("ProfilePicker.FREFlow.StepTotalDuration",
                                      expected_step_total_duration_count);

  RunTestSequence(
      If([]() { return WithSupervisedUser(); },
         Then(WaitForPromo(
             feature_engagement::kIPHSupervisedUserProfileSigninFeature)),
         Else(EnsureNotPresent(
             user_education::HelpBubbleView::kHelpBubbleElementIdForTesting))));
}

IN_PROC_BROWSER_TEST_P(FirstRunParameterizedInteractiveUiTestWithSyncService,
                       DeclineSync) {
  bool should_skip_test = false;
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/366082752): Re-enable this test
  should_skip_test = true;
#endif  // WIN
  if (should_skip_test) {
    GTEST_SKIP() << "Test is flaky on win64";
  }

  if (SyncButtonsFeatureConfig() ==
      SyncButtonsFeatureConfig::kButtonsStillLoading) {
    GTEST_SKIP() << "Decline is not possible until buttons stop loading";
  }
  auto iph_delay =
      AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
          base::Seconds(0));
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

  auto sync_transport_state =
      GetParam().with_sync_engine_ready
          ? syncer::SyncService::TransportState::ACTIVE
          : syncer::SyncService::TransportState::INITIALIZING;
  ConfigureTestSyncService(SyncServiceFactory::GetForProfile(profile()),
                           sync_transport_state);
  // Pulled out of the test sequence because it waits using `RunLoop`s.
  SimulateSignIn(kTestEmail, kTestGivenName);

  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    GURL history_page_url = GetHistorySyncOptinURL();
    RunTestSequenceInContext(
        views::ElementTrackerViews::GetContextForView(view()),
        // Web Contents already instrumented in the previous sequence.
        WaitForStateChange(kWebContentsId,
                           PageWithUrl(history_page_url.spec())),
        // Button is visible once capabilities are loaded or defaulted.
        WaitForButtonVisible(kWebContentsId, GetDontSyncHistoryButtonQuery()),

        EnsurePresent(kWebContentsId, GetDontSyncHistoryButtonQuery()),
        PressJsButton(kWebContentsId, GetDontSyncHistoryButtonQuery()),

        CompleteSearchEngineChoiceStep(),
        If([this]() { return !UseRevampedView(); },
           Then(CompleteDefaultBrowserStep())),
        If([this]() { return UseRevampedView(); },
           Then(CompleteFinishOrContinueStep())));
  } else {
    RunTestSequenceInContext(
        views::ElementTrackerViews::GetContextForView(view()),
        // Web Contents already instrumented in the previous sequence.
        WaitForStateChange(kWebContentsId,
                           PageWithUrl(AppendSyncConfirmationQueryParams(
                                           GURL("chrome://sync-confirmation/"),
                                           SyncConfirmationStyle::kWindow,
                                           /*is_sync_promo=*/true)
                                           .spec())),
        // Button is visible once capabilities are loaded or defaulted.
        WaitForButtonVisible(kWebContentsId, GetDontSyncButtonQuery()),

        EnsurePresent(kWebContentsId, GetDontSyncButtonQuery()),
        PressJsButton(kWebContentsId, GetDontSyncButtonQuery()),

        CompleteSearchEngineChoiceStep(),
        If([this]() { return !UseRevampedView(); },
           Then(CompleteDefaultBrowserStep())),
        If([this]() { return UseRevampedView(); },
           Then(CompleteFinishOrContinueStep())));
  }

  // Wait for the picker to be closed and deleted.
  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());

  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), GetProfileName());

  // Checking the expected metrics from this flow.
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kForYouFre, 1);
  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    histogram_tester().ExpectUniqueSample(
        "Signin.HistorySyncOptIn.Started",
        signin_metrics::AccessPoint::kForYouFre, 1);
    histogram_tester().ExpectTotalCount("Signin.HistorySyncOptIn.Completed", 0);
  } else {
    histogram_tester().ExpectUniqueSample(
        "Signin.SyncOptIn.Started", signin_metrics::AccessPoint::kForYouFre, 1);
    histogram_tester().ExpectTotalCount("Signin.SyncOptIn.Completed", 0);
  }
  histogram_tester().ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      *ExpectedButtonShownMetric(SyncButtonsFeatureConfig()), 1);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);

  RunTestSequence(
      If([]() { return WithSupervisedUser(); },
         Then(WaitForPromo(
             feature_engagement::kIPHSupervisedUserProfileSigninFeature)),
         Else(EnsureNotPresent(
             user_education::HelpBubbleView::kHelpBubbleElementIdForTesting))));
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunParameterizedInteractiveUiTestWithSyncService,
                         ValuesIn(GetTestParams()),
                         &ParamToTestSuffix);

struct HatsTestParams {
  FirstRunVersion::Value flow_version = FirstRunVersion::Legacy{};
  base::test::FeatureRef hats_feature;
  std::string_view hats_trigger;
  std::string_view test_suffix;
  std::vector<std::string> forced_showcase_steps;
};

const HatsTestParams kHatsTestParams[] = {
    {.flow_version = FirstRunVersion::Legacy{},
     .hats_feature = switches::kBeforeFirstRunDesktopRefreshSurvey,
     .hats_trigger = kHatsSurveyTriggerIdentityFirstRunCompleted,
     .test_suffix = "BeforeRefreshSurvey"},
    {.flow_version = FirstRunVersion::Refreshed{},
     .hats_feature = switches::kFirstRunDesktopRefreshSurvey,
     .hats_trigger = kHatsSurveyTriggerIdentityRefreshedFirstRunCompleted,
     .test_suffix = "RefreshSurvey"},
    {.flow_version = FirstRunVersion::Revamped{},
     .hats_feature = switches::kFirstRunDesktopRevampSurvey,
     .hats_trigger = kHatsSurveyTriggerFirstRunDesktopRevampCompleted,
     .test_suffix = "RevampSurvey",
     .forced_showcase_steps = {"default-browser"}},
    {.flow_version = FirstRunVersion::Revamped{},
     .hats_feature = switches::kFirstRunDesktopRevampNoFeatureShowcaseSurvey,
     .hats_trigger =
         kHatsSurveyTriggerFirstRunDesktopRevampNoFeatureShowcaseCompleted,
     .test_suffix = "RevampNoFeatureShowcaseSurvey",
     .forced_showcase_steps = {}}};

class FirstRunWithHatsInteractiveUiTest
    : public WithParamInterface<HatsTestParams>,
      public FirstRunInteractiveUiBaseTest {
 public:
  FirstRunWithHatsInteractiveUiTest()
      : FirstRunInteractiveUiBaseTest(
            TestParam{.flow_version = GetParam().flow_version},
            {{*GetParam().hats_feature, {}}}) {}

  void SetUpOnMainThread() override {
    FirstRunInteractiveUiBaseTest::SetUpOnMainThread();
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            CHECK_DEREF(browser()).profile(),
            base::BindRepeating(&BuildMockHatsService)));
  }

  void TearDownOnMainThread() override {
    FirstRunInteractiveUiBaseTest::TearDownOnMainThread();
    mock_hats_service_ = nullptr;
  }

 protected:
  MockHatsService& mock_hats_service() {
    return CHECK_DEREF(mock_hats_service_);
  }

  std::string hats_trigger() const {
    return std::string(GetParam().hats_trigger);
  }

  std::vector<std::string> GetForcedFeatureShowcaseSteps() const override {
    return GetParam().forced_showcase_steps;
  }

  bool IsFeatureShowcaseEligible() const {
    return !GetParam().forced_showcase_steps.empty();
  }

  InteractiveTestApi::MultiStep DeclineHistorySync() {
    if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
      return Steps(
          WaitForWebContentsNavigation(kWebContentsId,
                                       GetHistorySyncOptinURL()),
          WaitForButtonVisible(kWebContentsId, GetDontSyncHistoryButtonQuery()),
          EnsurePresent(kWebContentsId, GetDontSyncHistoryButtonQuery()),
          PressJsButton(kWebContentsId, GetDontSyncHistoryButtonQuery())
              .SetMustRemainVisible(false));
    }
    GURL sync_page_url = AppendSyncConfirmationQueryParams(
        GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow,
        /*is_sync_promo=*/true);
    return Steps(
        WaitForWebContentsNavigation(kWebContentsId, std::move(sync_page_url)),
        WaitForButtonVisible(kWebContentsId, GetDontSyncButtonQuery()),
        EnsurePresent(kWebContentsId, GetDontSyncButtonQuery()),
        PressJsButton(kWebContentsId, GetDontSyncButtonQuery())
            .SetMustRemainVisible(false));
  }

 private:
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
};

// TODO(crbug.com/366082752): Re-enable this test once the issue is fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DoNotLaunchHatsIfDeclineSignIn \
  DISABLED_DoNotLaunchHatsIfDeclineSignIn
#else
#define MAYBE_DoNotLaunchHatsIfDeclineSignIn DoNotLaunchHatsIfDeclineSignIn
#endif
IN_PROC_BROWSER_TEST_P(FirstRunWithHatsInteractiveUiTest,
                       MAYBE_DoNotLaunchHatsIfDeclineSignIn) {
  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(hats_trigger(), _, _, _))
      .Times(0);

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/false),
      If([this]() { return UseRevampedView(); },
         Then(If(
             [this]() { return IsFeatureShowcaseEligible(); },
             Then(Steps(
                 WaitForShow(kProfilePickerToolbarStartBrowsingButtonElementId),
                 PressButton(
                     kProfilePickerToolbarStartBrowsingButtonElementId))),
             Else(CompleteFinishOrContinueStep())))));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
}

IN_PROC_BROWSER_TEST_P(FirstRunWithHatsInteractiveUiTest,
                       DoNotLaunchHatsIfFlowNotCompleted) {
  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(hats_trigger(), _, _, _))
      .Times(0);

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      WaitForWebContentsReady(kWebContentsId, GURL(chrome::kChromeUIIntroURL)),
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_CLOSE_WINDOW))
          .SetMustRemainVisible(false));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  ExpectStepHistograms(Step::kIntro, /*shown=*/true, /*with_exit=*/true);
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunWithHatsInteractiveUiTest,
                         ValuesIn(kHatsTestParams),
                         [](const TestParamInfo<HatsTestParams>& info) {
                           return std::string(info.param.test_suffix);
                         });

using FirstRunWithHatsInteractiveUiTestWithSyncService =
    WithTestSyncServiceMixin<FirstRunWithHatsInteractiveUiTest>;

// TODO(crbug.com/366082752): Re-enable this test once the issue is fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_LaunchHats DISABLED_LaunchHats
#else
#define MAYBE_LaunchHats LaunchHats
#endif
IN_PROC_BROWSER_TEST_P(FirstRunWithHatsInteractiveUiTestWithSyncService,
                       MAYBE_LaunchHats) {
  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  const std::map<std::string, std::string> survey_data = {
      {"Channel", "unknown"}};
  EXPECT_CALL(mock_hats_service(),
              LaunchDelayedSurvey(hats_trigger(), _, _, Eq(survey_data)));
  // No other survey should be launched (e.g. permanent identity FRE survey).
  EXPECT_CALL(mock_hats_service(),
              LaunchDelayedSurvey(Not(hats_trigger()), _, _, _))
      .Times(0);

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  ConfigureTestSyncService(SyncServiceFactory::GetForProfile(profile()),
                           syncer::SyncService::TransportState::ACTIVE);
  SimulateSignIn(kTestEmail, kTestGivenName);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      If([this]() { return UseRevampedView(); },
         Then(WaitForWebContentsNavigation(
             kWebContentsId,
             GURL(chrome::kChromeUIIntroURL)
                 .Resolve(chrome::kChromeUIIntroSignInCelebrationSubPage)))),
      DeclineHistorySync(),
      If([this]() { return UseRevampedView(); },
         Then(If(
             [this]() { return IsFeatureShowcaseEligible(); },
             Then(Steps(
                 WaitForShow(kProfilePickerToolbarStartBrowsingButtonElementId),
                 PressButton(
                     kProfilePickerToolbarStartBrowsingButtonElementId))),
             Else(CompleteFinishOrContinueStep())))));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
}

// TODO(crbug.com/366082752): Re-enable this test once the issue is fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DoNotLaunchHatsIfEnterpriseUser \
  DISABLED_DoNotLaunchHatsIfEnterpriseUser
#else
#define MAYBE_DoNotLaunchHatsIfEnterpriseUser DoNotLaunchHatsIfEnterpriseUser
#endif
IN_PROC_BROWSER_TEST_P(FirstRunWithHatsInteractiveUiTestWithSyncService,
                       MAYBE_DoNotLaunchHatsIfEnterpriseUser) {
  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(
                     &policy::FakeUserPolicySigninService::BuildForEnterprise));

  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(hats_trigger(), _, _, _))
      .Times(0);

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  ConfigureTestSyncService(SyncServiceFactory::GetForProfile(profile()),
                           syncer::SyncService::TransportState::ACTIVE);
  SimulateSignIn(kTestEnterpriseEmail, kTestGivenName,
                 /*with_extended_info=*/false);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForWebContentsNavigation(
          kWebContentsId,
          AppendSyncConfirmationQueryParams(
              GURL(chrome::kChromeUISyncConfirmationURL)
                  .Resolve(chrome::kChromeUISyncConfirmationLoadingPath),
              SyncConfirmationStyle::kWindow, /*is_sync_promo=*/true)));

  auto& identity_manager =
      CHECK_DEREF(IdentityManagerFactory::GetForProfile(profile()));

  AccountInfo account_info =
      identity_manager.FindExtendedAccountInfoByAccountId(
          identity_manager.GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  account_info = signin::WithGeneratedUserInfo(account_info, kTestGivenName);
  account_info = AccountInfo::Builder(account_info)
                     .SetHostedDomain("chromium.org")
                     .Build();
  signin::UpdateAccountInfoForAccount(&identity_manager,
                                      std::move(account_info));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForWebContentsNavigation(
          kWebContentsId,
          UseRefreshedView()
              ? ManagedUserProfileNoticeUI::GetURLForType(
                    ManagedUserProfileNoticeUI::ScreenType::kFirstRun)
              : GURL(chrome::kChromeUIManagedUserProfileNoticeUrl)),
      EnsurePresent(kWebContentsId, GetAcceptManagementButtonQuery()),
      PressJsButton(kWebContentsId, GetAcceptManagementButtonQuery()),
      DeclineHistorySync(),
      If([this]() { return UseRevampedView(); },
         Then(If(
             [this]() { return IsFeatureShowcaseEligible(); },
             Then(Steps(
                 WaitForShow(kProfilePickerToolbarStartBrowsingButtonElementId),
                 PressButton(
                     kProfilePickerToolbarStartBrowsingButtonElementId))),
             Else(CompleteFinishOrContinueStep())))));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunWithHatsInteractiveUiTestWithSyncService,
                         ValuesIn(kHatsTestParams),
                         [](const TestParamInfo<HatsTestParams>& info) {
                           return std::string(info.param.test_suffix);
                         });

using FirstRunWithHatsAndUnrelatedFeatureSetInteractiveUiTest =
    FirstRunWithHatsInteractiveUiTestWithSyncService;

// TODO(crbug.com/366082752): Re-enable this test once the issue is fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DoNotLaunchHats DISABLED_DoNotLaunchHats
#else
#define MAYBE_DoNotLaunchHats DoNotLaunchHats
#endif
IN_PROC_BROWSER_TEST_P(FirstRunWithHatsAndUnrelatedFeatureSetInteractiveUiTest,
                       MAYBE_DoNotLaunchHats) {
  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  // A survey with the HaTS trigger should not be launched for a not matching
  // feature set.
  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(hats_trigger(), _, _, _))
      .Times(0);

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  ConfigureTestSyncService(SyncServiceFactory::GetForProfile(profile()),
                           syncer::SyncService::TransportState::ACTIVE);
  SimulateSignIn(kTestEmail, kTestGivenName);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      If([this]() { return UseRevampedView(); },
         Then(WaitForWebContentsNavigation(
             kWebContentsId,
             GURL(chrome::kChromeUIIntroURL)
                 .Resolve(chrome::kChromeUIIntroSignInCelebrationSubPage)))),
      DeclineHistorySync(),
      If([this]() { return UseRevampedView(); },
         Then(If(
             [this]() { return IsFeatureShowcaseEligible(); },
             Then(Steps(
                 WaitForShow(kProfilePickerToolbarStartBrowsingButtonElementId),
                 PressButton(
                     kProfilePickerToolbarStartBrowsingButtonElementId))),
             Else(CompleteFinishOrContinueStep())))));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FirstRunWithHatsAndUnrelatedFeatureSetInteractiveUiTest,
    Values(
        HatsTestParams{
            .flow_version = FirstRunVersion::Refreshed{},
            .hats_feature = switches::kBeforeFirstRunDesktopRefreshSurvey,
            .hats_trigger = kHatsSurveyTriggerIdentityFirstRunCompleted,
            .test_suffix = "BeforeRefreshSurvey"},
        HatsTestParams{.flow_version = FirstRunVersion::Legacy{},
                       .hats_feature = switches::kFirstRunDesktopRefreshSurvey,
                       .hats_trigger =
                           kHatsSurveyTriggerIdentityRefreshedFirstRunCompleted,
                       .test_suffix = "RefreshSurvey"},
        HatsTestParams{.flow_version = FirstRunVersion::Revamped{},
                       .hats_feature = switches::kFirstRunDesktopRefreshSurvey,
                       .hats_trigger =
                           kHatsSurveyTriggerIdentityRefreshedFirstRunCompleted,
                       .test_suffix = "RefreshSurveyWithRevampFlow"},
        HatsTestParams{
            .flow_version = FirstRunVersion::Refreshed{},
            .hats_feature = switches::kFirstRunDesktopRevampSurvey,
            .hats_trigger = kHatsSurveyTriggerFirstRunDesktopRevampCompleted,
            .test_suffix = "RevampSurveyWithRefreshedFlow"}),
    [](const TestParamInfo<HatsTestParams>& info) {
      return std::string(info.param.test_suffix);
    });

class FirstRunDontSignInOnGaiaPageInteractiveUiTest
    : public FirstRunInteractiveUiBaseTest {
 public:
  FirstRunDontSignInOnGaiaPageInteractiveUiTest()
      : FirstRunInteractiveUiBaseTest(TestParam{
            .flow_version = FirstRunVersion::Refreshed{
                .variant = switches::FirstRunDesktopSignInPromoVariation::
                    kDontSignInOnGaiaPage}}) {}
};

// TODO(crbug.com/366119368): Re-enable this test
#if BUILDFLAG(IS_WIN)
#define MAYBE_DeclineSignInFromNativeToolbar \
  DISABLED_DeclineSignInFromNativeToolbar
#else
#define MAYBE_DeclineSignInFromNativeToolbar DeclineSignInFromNativeToolbar
#endif
IN_PROC_BROWSER_TEST_F(FirstRunDontSignInOnGaiaPageInteractiveUiTest,
                       MAYBE_DeclineSignInFromNativeToolbar) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()),
      WaitForShow(kProfilePickerToolbarDontSignInButtonElementId),
      PressButton(kProfilePickerToolbarDontSignInButtonElementId));

  WaitForPickerClosed();
  EXPECT_TRUE(proceed_future.Get());

  histogram_tester().ExpectUniqueSample(
      /*name=*/"Signin.SignIn.Offered",
      /*sample=*/signin_metrics::AccessPoint::kForYouFre,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Signin.SignIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kForYouFre,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"ProfilePicker.FirstRun.ExitStatus",
      /*sample=*/ProfilePicker::FirstRunExitStatus::kCompleted,
      /*expected_bucket_count=*/1);

  ExpectStepHistograms(Step::kIntro, /*shown=*/true);
  ExpectStepHistograms(Step::kAccountSelection, /*shown=*/true);
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
  // Sign-in was never completed - step hasn't been attempted.
  ExpectStepHistograms(Step::kPostSignInFlow, /*shown=*/false,
                       /*with_exit=*/false, /*count=*/0);
}

class FirstRunInSearchChoiceRegionInteractiveUiTest
    : public base::test::WithFeatureOverride,
      public FirstRunInteractiveUiBaseTest {
 public:
  FirstRunInSearchChoiceRegionInteractiveUiTest()
      : base::test::WithFeatureOverride(
            switches::kWaffleRestrictToAssociatedCountries) {
    scoped_chrome_build_override_ = std::make_unique<base::AutoReset<bool>>(
        SearchEngineChoiceDialogServiceFactory::
            ScopedChromeBuildOverrideForTesting(
                /*force_chrome_build=*/true));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    FirstRunInteractiveUiBaseTest::SetUpCommandLine(command_line);
    SetUpCommandLineForChoiceScreen(command_line);
  }

 private:
  std::unique_ptr<base::AutoReset<bool>> scoped_chrome_build_override_;
};

// TODO(crbug.com/366119368): Re-enable this test. (FRE does not open on Win)
#if BUILDFLAG(IS_WIN)
#define MAYBE_SkipChoiceScreenDynamically DISABLED_SkipChoiceScreenDynamically
#else
#define MAYBE_SkipChoiceScreenDynamically SkipChoiceScreenDynamically
#endif
IN_PROC_BROWSER_TEST_P(FirstRunInSearchChoiceRegionInteractiveUiTest,
                       MAYBE_SkipChoiceScreenDynamically) {
  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  auto* dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());

  ASSERT_TRUE(dialog_service);  // The service should be created, indicating we
                                // can prompt the user.

  EXPECT_EQ(
      regional_capabilities::SearchEngineChoiceScreenConditions::kEligible,
      dialog_service->ComputeProfileManagementFlowConditions());

  // Set the DSE to a custom search engine, which should result in an ineligible
  // dynamic condition.
  TemplateURLData custom_search_data;
  custom_search_data.SetShortName(u"codesearch");
  custom_search_data.SetKeyword(u"cs");
  custom_search_data.SetURL("https://search.chromium.org?q={searchTerms}");
  TemplateURL custom_search_engine(custom_search_data);
  TemplateURLServiceFactory::GetForProfile(profile())
      ->SetUserSelectedDefaultSearchProvider(&custom_search_engine);

  ASSERT_EQ(regional_capabilities::SearchEngineChoiceScreenConditions::
                kHasCustomSearchEngine,
            dialog_service->ComputeProfileManagementFlowConditions());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/false),
      If([this]() { return !IsParamFeatureEnabled(); },
         Then(CompleteSearchEngineChoiceStep())));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::
          kFreChoiceScreenWasDisplayed,
      IsParamFeatureEnabled() ? 0 : 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunInSearchChoiceRegionInteractiveUiTest,
                         testing::Values(false, true));

// TODO(crbug.com/524526106): Extend this test suite to thoroughly cover the
// feature showcase step.
class FirstRunRevampInteractiveUiTest : public FirstRunInteractiveUiBaseTest {
 public:
  explicit FirstRunRevampInteractiveUiTest(
      const std::vector<base::test::FeatureRefAndParams>&
          fixture_enabled_features = {},
      const std::vector<base::test::FeatureRef>& fixture_disabled_features = {})
      : FirstRunInteractiveUiBaseTest(
            TestParam{.flow_version = FirstRunVersion::Revamped{}},
            fixture_enabled_features,
            fixture_disabled_features) {}

 protected:
  GURL GetFeatureShowcaseUrl() const {
    return net::AppendQueryParameter(
        GURL(chrome::kChromeUIFeatureShowcaseURL), "steps",
        base::JoinString(GetForcedFeatureShowcaseSteps(), ","));
  }

  const DeepQuery& GetFeatureShowcaseDefaultBrowserSkipButtonQuery() const {
    static const base::NoDestructor<DeepQuery> kQuery(
        {"feature-showcase-app", "feature-showcase-default-browser-step",
         "#skip-button"});
    return *kQuery;
  }

  const DeepQuery& GetFeatureShowcaseGoogleLensSkipButtonQuery() const {
    static const base::NoDestructor<DeepQuery> kQuery(
        {"feature-showcase-app", "feature-showcase-google-lens-step",
         "#skip-button"});
    return *kQuery;
  }

  // FirstRunInteractiveUiBaseTest:
  std::vector<std::string> GetForcedFeatureShowcaseSteps() const override {
    return {"default-browser", "google-lens"};
  }
};

IN_PROC_BROWSER_TEST_F(FirstRunRevampInteractiveUiTest, InitSoundsOnFlowStart) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  auto mock_sounds_manager = std::make_unique<StrictMock<MockSoundsManager>>();
  MockSoundsManager* mock_sounds_manager_ptr = mock_sounds_manager.get();

  // Set the factory to return the `MockSoundsManager`.
  base::AutoReset<FirstRunFlowController::SoundsManagerFactory>
      sounds_factory_reset =
          FirstRunFlowController::SetSoundsManagerFactoryForTesting(
              base::BindLambdaForTesting(
                  [&mock_sounds_manager](
                      audio::SoundsManager::StreamFactoryBinder)
                      -> std::unique_ptr<audio::SoundsManager> {
                    return std::move(mock_sounds_manager);
                  }));

  // Verify that the ambient, logo, welcome back and feature showcase sounds are
  // initialized at the start.
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Initialize(FirstRunFlowController::kAmbientSoundKey,
                         IDR_INTRO_SOUND_AMBIENT_FLAC, media::AudioCodec::kFLAC,
                         /*loop=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Initialize(FirstRunFlowController::kLogoSoundKey,
                         IDR_INTRO_SOUND_LOGO_FLAC, media::AudioCodec::kFLAC,
                         /*loop=*/false))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Initialize(FirstRunFlowController::kWelcomeBackSoundKey,
                         IDR_INTRO_SOUND_WELCOME_BACK_FLAC,
                         media::AudioCodec::kFLAC, /*loop=*/false))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_sounds_manager_ptr,
      Initialize(FirstRunFlowController::kFeatureShowcaseAmbientSoundKey,
                 IDR_INTRO_SOUND_FEATURE_SHOWCASE_AMBIENT_FLAC,
                 media::AudioCodec::kFLAC, /*loop=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_sounds_manager_ptr,
      Initialize(FirstRunFlowController::kFeatureShowcaseProgressSoundKey,
                 IDR_INTRO_SOUND_FEATURE_SHOWCASE_PROGRESS_FLAC,
                 media::AudioCodec::kFLAC,
                 /*loop=*/false));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Initialize(FirstRunFlowController::kAllSetSoundKey,
                         IDR_INTRO_SOUND_ALL_SET_FLAC, media::AudioCodec::kFLAC,
                         /*loop=*/false))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kAmbientSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kLogoSoundKey))
      .WillOnce(Return(true));

  OpenFirstRun();
}

IN_PROC_BROWSER_TEST_F(FirstRunRevampInteractiveUiTest,
                       EffectsButtonControlsSounds) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  auto mock_sounds_manager = std::make_unique<StrictMock<MockSoundsManager>>();
  MockSoundsManager* mock_sounds_manager_ptr = mock_sounds_manager.get();

  // Set the factory to return the `MockSoundsManager`.
  base::AutoReset<FirstRunFlowController::SoundsManagerFactory>
      sounds_factory_reset =
          FirstRunFlowController::SetSoundsManagerFactoryForTesting(
              base::BindLambdaForTesting(
                  [&mock_sounds_manager](
                      audio::SoundsManager::StreamFactoryBinder)
                      -> std::unique_ptr<audio::SoundsManager> {
                    return std::move(mock_sounds_manager);
                  }));

  EXPECT_CALL(*mock_sounds_manager_ptr, Initialize)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr, Play)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  OpenFirstRun();

  Mock::VerifyAndClearExpectations(mock_sounds_manager_ptr);

  // Verify that clicking on the effects control button pauses
  // the ambient sound, and stops the one-shot sound(s).
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Pause(FirstRunFlowController::kAmbientSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Stop(FirstRunFlowController::kLogoSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Stop(FirstRunFlowController::kWelcomeBackSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Stop(FirstRunFlowController::kFeatureShowcaseProgressSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Stop(FirstRunFlowController::kAllSetSoundKey))
      .WillOnce(Return(true));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      // Wait for the effects control button to be visible.
      WaitForShow(kProfilePickerToolbarEffectsControlButtonElementId),
      // Click the effects control button. Since it is currently playing, it
      // should pause.
      PressButton(kProfilePickerToolbarEffectsControlButtonElementId));

  Mock::VerifyAndClearExpectations(mock_sounds_manager_ptr);

  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FREFlow.MediaEffects.Disable", Step::kIntro, 1);

  // Verify that clicking on the effects control button resumes the ambient
  // sound, and DOES NOT play the one-shot sound(s) again.
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kAmbientSoundKey))
      .WillOnce(Return(true));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      PressButton(kProfilePickerToolbarEffectsControlButtonElementId));

  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FREFlow.MediaEffects.Enable", Step::kIntro, 1);
}

class FirstRunRevampPostSignInInteractiveUiTest
    : public FirstRunRevampInteractiveUiTest,
      public testing::WithParamInterface<bool> {
 public:
  FirstRunRevampPostSignInInteractiveUiTest()
      : FirstRunRevampInteractiveUiTest(
            /*fixture_enabled_features=*/{
                {syncer::kReplaceSyncPromosWithSignInPromos, {}}}) {}

 protected:
  void SetUpOnMainThread() override {
    FirstRunRevampInteractiveUiTest::SetUpOnMainThread();
    if (is_managed()) {
      policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
          profile(),
          base::BindRepeating(
              &policy::FakeUserPolicySigninService::BuildForEnterprise));
    }
  }

  bool is_managed() const { return GetParam(); }

  const std::string& GetTestEmail() const {
    return is_managed() ? kTestEnterpriseEmail : kTestEmail;
  }

  GURL GetPostSignInPageUrl() const {
    if (is_managed()) {
      return ManagedUserProfileNoticeUI::GetURLForType(
          ManagedUserProfileNoticeUI::ScreenType::kFirstRun);
    }
    return GURL(chrome::kChromeUIIntroURL)
        .Resolve(chrome::kChromeUIIntroSignInCelebrationSubPage);
  }
};

IN_PROC_BROWSER_TEST_P(FirstRunRevampPostSignInInteractiveUiTest,
                       WelcomeBackSoundPlayed) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  auto mock_sounds_manager = std::make_unique<StrictMock<MockSoundsManager>>();
  MockSoundsManager* mock_sounds_manager_ptr = mock_sounds_manager.get();

  base::AutoReset<FirstRunFlowController::SoundsManagerFactory>
      sounds_factory_reset =
          FirstRunFlowController::SetSoundsManagerFactoryForTesting(
              base::BindLambdaForTesting(
                  [&mock_sounds_manager](
                      audio::SoundsManager::StreamFactoryBinder)
                      -> std::unique_ptr<audio::SoundsManager> {
                    return std::move(mock_sounds_manager);
                  }));

  EXPECT_CALL(*mock_sounds_manager_ptr, Initialize)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr, Play)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/true),
      // Wait for switch to Gaia sign-in page.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  Mock::VerifyAndClearExpectations(mock_sounds_manager_ptr);

  // Verify that the welcome back sound is played.
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kWelcomeBackSoundKey))
      .WillOnce(Return(true));

  SimulateSignIn(GetTestEmail(), kTestGivenName);

  // Wait for the first post sign-in page to load and trigger the sound play
  // call.
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForWebContentsNavigation(kWebContentsId, GetPostSignInPageUrl()));
}

IN_PROC_BROWSER_TEST_P(FirstRunRevampPostSignInInteractiveUiTest,
                       WelcomeBackSoundNotPlayedWhenEffectsPaused) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  auto mock_sounds_manager = std::make_unique<StrictMock<MockSoundsManager>>();
  MockSoundsManager* mock_sounds_manager_ptr = mock_sounds_manager.get();

  base::AutoReset<FirstRunFlowController::SoundsManagerFactory>
      sounds_factory_reset =
          FirstRunFlowController::SetSoundsManagerFactoryForTesting(
              base::BindLambdaForTesting(
                  [&mock_sounds_manager](
                      audio::SoundsManager::StreamFactoryBinder)
                      -> std::unique_ptr<audio::SoundsManager> {
                    return std::move(mock_sounds_manager);
                  }));

  EXPECT_CALL(*mock_sounds_manager_ptr, Initialize)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(Not(FirstRunFlowController::kWelcomeBackSoundKey)))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  // Expect pause/stop calls.
  EXPECT_CALL(*mock_sounds_manager_ptr, Pause)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr, Stop)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  const DeepQuery& sign_in_button = GetSignInButtonQuery();
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      // Wait for the effects control button to be visible.
      WaitForShow(kProfilePickerToolbarEffectsControlButtonElementId),
      // Click the effects control button to pause the effects.
      PressButton(kProfilePickerToolbarEffectsControlButtonElementId),
      WaitForStateChange(kWebContentsId, IsVisible(sign_in_button)),
      PressJsButton(kWebContentsId, sign_in_button),
      // Wait for switch to Gaia sign-in page.
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  SimulateSignIn(GetTestEmail(), kTestGivenName);

  // Wait for the final page to load. Since effects are paused, the sound
  // should not be played (enforced by `StrictMock`).
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForWebContentsNavigation(kWebContentsId, GetPostSignInPageUrl()));
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunRevampPostSignInInteractiveUiTest,
                         Bool(),
                         [](const TestParamInfo<bool>& info) {
                           return info.param ? "Managed" : "Unmanaged";
                         });

IN_PROC_BROWSER_TEST_F(FirstRunRevampInteractiveUiTest, FeatureShowcaseSound) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  auto mock_sounds_manager = std::make_unique<StrictMock<MockSoundsManager>>();
  MockSoundsManager* mock_sounds_manager_ptr = mock_sounds_manager.get();

  base::AutoReset<FirstRunFlowController::SoundsManagerFactory>
      sounds_factory_reset =
          FirstRunFlowController::SetSoundsManagerFactoryForTesting(
              base::BindLambdaForTesting(
                  [&mock_sounds_manager](
                      audio::SoundsManager::StreamFactoryBinder)
                      -> std::unique_ptr<audio::SoundsManager> {
                    return std::move(mock_sounds_manager);
                  }));

  EXPECT_CALL(*mock_sounds_manager_ptr, Initialize)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kAmbientSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kLogoSoundKey))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()));

  Mock::VerifyAndClearExpectations(mock_sounds_manager_ptr);

  // When entering the feature showcase, we expect ambient sound to stop and
  // feature showcase ambient sound to play. We do NOT expect the progress sound
  // to play on the first step of the feature showcase (enforced by
  // `StrictMock`).
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Stop(FirstRunFlowController::kAmbientSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kFeatureShowcaseAmbientSoundKey))
      .WillOnce(Return(true));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      CompleteIntroStep(/*sign_in=*/false),
      WaitForWebContentsNavigation(kWebContentsId, GetFeatureShowcaseUrl()));

  Mock::VerifyAndClearExpectations(mock_sounds_manager_ptr);

  // When transitioning to the next step inside the feature showcase, we expect
  // the progress sound to play.
  base::RunLoop progress_run_loop;
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kFeatureShowcaseProgressSoundKey))
      .WillOnce([&progress_run_loop](audio::SoundsManager::SoundKey key) {
        progress_run_loop.Quit();
        return true;
      });

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForButtonVisible(kWebContentsId,
                           GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      EnsurePresent(kWebContentsId,
                    GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      PressJsButton(kWebContentsId,
                    GetFeatureShowcaseDefaultBrowserSkipButtonQuery()));

  progress_run_loop.Run();
  Mock::VerifyAndClearExpectations(mock_sounds_manager_ptr);

  // When leaving the feature showcase, we expect feature showcase ambient sound
  // to stop and ambient sound to play.
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Stop(FirstRunFlowController::kFeatureShowcaseAmbientSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kAmbientSoundKey))
      .WillOnce(Return(true));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerToolbarStartBrowsingButtonElementId),
      PressButton(kProfilePickerToolbarStartBrowsingButtonElementId));

  WaitForPickerClosed();
  EXPECT_TRUE(proceed_future.Get());
}

IN_PROC_BROWSER_TEST_F(FirstRunRevampInteractiveUiTest,
                       StartBrowsingFromFeatureShowcase) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      // Do not sign in to proceed to the feature showcase immediately.
      CompleteIntroStep(/*sign_in=*/false),
      WaitForWebContentsNavigation(kWebContentsId, GetFeatureShowcaseUrl()));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "ProfilePicker.FREFlow.FeatureShowcase.StepEligible"),
              BucketsAre(Bucket(FeatureShowcaseStep::kDefaultBrowser, 1),
                         Bucket(FeatureShowcaseStep::kGoogleLens, 1)));

  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepShown",
      FeatureShowcaseStep::kDefaultBrowser, 1);
  histogram_tester().ExpectTotalCount(
      "ProfilePicker.FREFlow.FeatureShowcase.StartBrowsing", 0);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerToolbarStartBrowsingButtonElementId),
      PressButton(kProfilePickerToolbarStartBrowsingButtonElementId));

  WaitForPickerClosed();
  EXPECT_TRUE(proceed_future.Get());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  ExpectStepHistograms(Step::kIntro, /*shown=*/true);
  ExpectStepHistograms(Step::kFeatureShowcase, /*shown=*/true);
  ExpectStepHistograms(Step::kFinishFlow, /*shown=*/true, /*with_exit=*/true);

  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepShown",
      FeatureShowcaseStep::kDefaultBrowser, 1);
  histogram_tester().ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StartBrowsing",
      FeatureShowcaseStep::kDefaultBrowser, 1);
}

IN_PROC_BROWSER_TEST_F(FirstRunRevampInteractiveUiTest,
                       AllSetSoundPlaysOnFinishOrContinueStep) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  auto mock_sounds_manager = std::make_unique<StrictMock<MockSoundsManager>>();
  MockSoundsManager* mock_sounds_manager_ptr = mock_sounds_manager.get();

  base::AutoReset<FirstRunFlowController::SoundsManagerFactory>
      sounds_factory_reset =
          FirstRunFlowController::SetSoundsManagerFactoryForTesting(
              base::BindLambdaForTesting(
                  [&mock_sounds_manager](
                      audio::SoundsManager::StreamFactoryBinder)
                      -> std::unique_ptr<audio::SoundsManager> {
                    return std::move(mock_sounds_manager);
                  }));

  EXPECT_CALL(*mock_sounds_manager_ptr, Initialize)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(Not(FirstRunFlowController::kAllSetSoundKey)))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(FirstRunFlowController::kAllSetSoundKey))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr, Stop)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/false),
      WaitForWebContentsNavigation(kWebContentsId, GetFeatureShowcaseUrl()),
      WaitForButtonEnabled(kWebContentsId,
                           GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      EnsurePresent(kWebContentsId,
                    GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      PressJsButton(kWebContentsId,
                    GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      WaitForButtonEnabled(kWebContentsId,
                           GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      EnsurePresent(kWebContentsId,
                    GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      PressJsButton(kWebContentsId,
                    GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      CompleteFinishOrContinueStep());

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
}

IN_PROC_BROWSER_TEST_F(FirstRunRevampInteractiveUiTest,
                       AllSetSoundDoesNotPlayWhenEffectsPaused) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  auto mock_sounds_manager = std::make_unique<StrictMock<MockSoundsManager>>();
  MockSoundsManager* mock_sounds_manager_ptr = mock_sounds_manager.get();

  base::AutoReset<FirstRunFlowController::SoundsManagerFactory>
      sounds_factory_reset =
          FirstRunFlowController::SetSoundsManagerFactoryForTesting(
              base::BindLambdaForTesting(
                  [&mock_sounds_manager](
                      audio::SoundsManager::StreamFactoryBinder)
                      -> std::unique_ptr<audio::SoundsManager> {
                    return std::move(mock_sounds_manager);
                  }));

  EXPECT_CALL(*mock_sounds_manager_ptr, Initialize)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr,
              Play(Not(FirstRunFlowController::kAllSetSoundKey)))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr, Stop)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sounds_manager_ptr, Pause)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      CompleteIntroStep(/*sign_in=*/false),
      WaitForWebContentsNavigation(kWebContentsId, GetFeatureShowcaseUrl()),
      WaitForButtonEnabled(kWebContentsId,
                           GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      EnsurePresent(kWebContentsId,
                    GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      PressJsButton(kWebContentsId,
                    GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      // Pause effects.
      WaitForShow(kProfilePickerToolbarEffectsControlButtonElementId),
      PressButton(kProfilePickerToolbarEffectsControlButtonElementId),
      WaitForButtonEnabled(kWebContentsId,
                           GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      EnsurePresent(kWebContentsId,
                    GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      PressJsButton(kWebContentsId,
                    GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      CompleteFinishOrContinueStep());

  WaitForPickerClosed();

  // Since effects are paused, the all set sound should not be played (enforced
  // by `StrictMock`).
  EXPECT_TRUE(proceed_future.Get());
}

IN_PROC_BROWSER_TEST_F(FirstRunRevampInteractiveUiTest,
                       ClickContinueEducationOpenWhatsNew) {
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  base::test::TestFuture<bool> proceed_future;
  OpenFirstRun(proceed_future.GetCallback());
  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      // Do not sign in to proceed to the feature showcase.
      CompleteIntroStep(/*sign_in=*/false),
      // Complete the feature showcase steps.
      WaitForWebContentsNavigation(kWebContentsId, GetFeatureShowcaseUrl()),
      WaitForButtonEnabled(kWebContentsId,
                           GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      EnsurePresent(kWebContentsId,
                    GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      PressJsButton(kWebContentsId,
                    GetFeatureShowcaseDefaultBrowserSkipButtonQuery()),
      WaitForButtonEnabled(kWebContentsId,
                           GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      EnsurePresent(kWebContentsId,
                    GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      PressJsButton(kWebContentsId,
                    GetFeatureShowcaseGoogleLensSkipButtonQuery()),
      // Choose to continue education on the finish or continue step.
      CompleteFinishOrContinueStep(/*start_browsing=*/false));

  WaitForPickerClosed();
  EXPECT_TRUE(proceed_future.Get());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());

  // Verify that the browser was opened with only the archive page tab.
  ASSERT_TRUE(browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(whats_new::kChromeWhatsNewURL).Resolve("archive/"));
}

class FirstRunRevampTurnOnSyncCelebrationInteractiveUiTest
    : public FirstRunRevampInteractiveUiTest {
 public:
  FirstRunRevampTurnOnSyncCelebrationInteractiveUiTest()
      : FirstRunRevampInteractiveUiTest(
            /*fixture_enabled_features=*/{},
            /*fixture_disabled_features=*/
            {syncer::kReplaceSyncPromosWithSignInPromos,
             syncer::kReplaceSyncPromosWithSigninPromosNewSignin}) {}

 protected:
  std::vector<std::string> GetForcedFeatureShowcaseSteps() const override {
    return {"default-browser"};
  }
};

using FirstRunRevampTurnOnSyncCelebrationInteractiveUiTestWithSyncService =
    WithTestSyncServiceMixin<
        FirstRunRevampTurnOnSyncCelebrationInteractiveUiTest>;

IN_PROC_BROWSER_TEST_F(
    FirstRunRevampTurnOnSyncCelebrationInteractiveUiTestWithSyncService,
    SignInSyncDisabledBypassesCelebrationShowsSyncDisabledNotice) {
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
      WaitForWebContentsNavigation(kWebContentsId,
                                   GetSigninChromeSyncDiceUrl()));

  // Configure SyncService to be disabled by policy.
  auto* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile()));
  ASSERT_TRUE(sync_service);
  sync_service->SetAllowedByEnterprisePolicy(false);

  SimulateSignIn(kTestEmail, kTestGivenName);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),
      // Verify we navigate directly to the sync disabled notice screen,
      // bypassing Sign-in Celebration.
      WaitForWebContentsNavigation(
          kWebContentsId,
          ManagedUserProfileNoticeUI::GetURLForType(
              ManagedUserProfileNoticeUI::ScreenType::
                  kConsumerAccountSyncDisabled)),
      WaitForStateChange(
          kWebContentsId,
          IsVisible({"managed-user-profile-notice-app-refresh"})),
      EnsurePresent(kWebContentsId, GetAcceptManagementButtonQuery()),
      PressJsButton(kWebContentsId, GetAcceptManagementButtonQuery()),

      // Complete the feature showcase step.
      WaitForWebContentsNavigation(kWebContentsId, GetFeatureShowcaseUrl()),
      WaitForShow(kProfilePickerToolbarStartBrowsingButtonElementId),
      PressButton(kProfilePickerToolbarStartBrowsingButtonElementId));

  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());
}
