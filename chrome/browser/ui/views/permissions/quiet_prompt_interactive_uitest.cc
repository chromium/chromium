// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_ui_selector.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/gurl.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
const char kLocationBarView[] = "LocationBarView";
const auto QuietChipElementId = PermissionChipView::kElementIdForTesting;
const auto QuietBubbleAllowElementId =
    views::DialogClientView::kOkButtonElementId;
const auto QuietBubbleElementId = ContentSettingBubbleContents::kMainElementId;
const auto InfobarElementId = ConfirmInfoBar::kInfoBarElementId;
using ::base::test::ScopedFeatureList;
using ::testing::ValuesIn;

class QuietPromptInteractiveUITest : public InteractiveBrowserTest {
 public:
  QuietPromptInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~QuietPromptInteractiveUITest() override = default;

  void SetUpOnMainThread() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());

    // Override url in the omnibox to avoid test flakiness due to different port
    // in the original url.
    std::u16string url_override(u"https://www.test.com/");
    OverrideVisibleUrlInLocationBar(url_override);

    InteractiveBrowserTest::SetUpOnMainThread();
  }
  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set a window's size to avoid pixel tests flakiness due to different
    // widths of the omnibox.
    command_line->AppendSwitchASCII(switches::kWindowSize,
                                    base::StringPrintf("%d,%d", 800, 600));
    InteractiveBrowserTest::SetUpCommandLine(command_line);
  }

  LocationBarView* GetLocationBarView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar();
  }

  void OverrideVisibleUrlInLocationBar(const std::u16string& text) {
    OmniboxView* omnibox_view = GetLocationBarView()->GetOmniboxView();
    raw_ptr<TestLocationBarModel> test_location_bar_model_ =
        new TestLocationBarModel;
    std::unique_ptr<LocationBarModel> location_bar_model(
        test_location_bar_model_);
    browser()->GetFeatures().swap_location_bar_models(&location_bar_model);

    test_location_bar_model_->set_formatted_full_url(text);

    // Normally the URL for display has portions elided. We aren't doing that in
    // this case, because that is irrevelant for these tests.
    test_location_bar_model_->set_url_for_display(text);

    omnibox_view->Update();
  }

  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using Decision = permissions::PermissionUiSelector::Decision;

  void SetCannedUiDecision(const Decision& decision) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<MockPermissionUiSelector>(decision));
  }

  // Checks that the permission chip is visible and in the given mode.
  // If `is_request` is false, should be in indicator mode instead.
  auto CheckChipIsRequest(bool is_request) {
    return CheckViewProperty(QuietChipElementId,
                             &PermissionChipView::GetIsRequestForTesting,
                             is_request);
  }

  auto CheckChipText(int id_string) {
    return CheckViewProperty(QuietChipElementId, &PermissionChipView::GetText,
                             l10n_util::GetStringUTF16(id_string));
  }

  auto CheckQuietPromptMessage(int id_string) {
    return CheckViewProperty(
        QuietBubbleElementId,
        &ContentSettingBubbleContents::get_message_for_test,
        l10n_util::GetStringUTF16(id_string));
  }

  permissions::PermissionActionsHistory* GetPermissionActionsHistory() {
    return permissions::PermissionsClient::Get()->GetPermissionActionsHistory(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetBrowserContext());
  }

  const base::HistogramTester& HistogramTester() const {
    return histogram_tester_;
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Notification1DenyRequestChipTest) {
  SetCannedUiDecision(
      Decision::UseQuietUi(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                           Decision::ShowNoWarning()));
  permissions::PermissionActionsHistory* permission_actions_history =
      GetPermissionActionsHistory();

  // 1 deny = Crowd Deny message.
  permission_actions_history->RecordAction(
      permissions::PermissionAction::DENIED,
      permissions::RequestType::kNotifications,
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(QuietChipElementId),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      WaitForShow(QuietChipElementId), CheckChipIsRequest(true),
      CheckChipText(IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_TITLE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "RequestChip", "5875965"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Notification1DenyPromptTest) {
  SetCannedUiDecision(
      Decision::UseQuietUi(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                           Decision::ShowNoWarning()));
  permissions::PermissionActionsHistory* permission_actions_history =
      GetPermissionActionsHistory();

  // 1 deny = Crowd Deny message.
  permission_actions_history->RecordAction(
      permissions::PermissionAction::DENIED,
      permissions::RequestType::kNotifications,
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(QuietChipElementId),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      WaitForShow(QuietChipElementId), CheckChipIsRequest(true),
      CheckChipText(IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_TITLE),
      EnsureNotPresent(QuietBubbleElementId), PressButton(QuietChipElementId),
      WaitForShow(QuietBubbleElementId),
      CheckQuietPromptMessage(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_PREDICTION_SERVICE_DESCRIPTION),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(QuietBubbleElementId, "QuietPromptPopupBubble",
                        "5934206"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Notification5DeniesPromptTest) {
  SetCannedUiDecision(
      Decision::UseQuietUi(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                           Decision::ShowNoWarning()));
  permissions::PermissionActionsHistory* permission_actions_history =
      GetPermissionActionsHistory();

  // 5 denies = CPSS message.
  for (int i = 0; i < 5; i++) {
    permission_actions_history->RecordAction(
        permissions::PermissionAction::DENIED,
        permissions::RequestType::kNotifications,
        permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
  }

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(QuietChipElementId),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      WaitForShow(QuietChipElementId), CheckChipIsRequest(true),
      CheckChipText(IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_TITLE),
      EnsureNotPresent(QuietBubbleElementId), PressButton(QuietChipElementId),
      WaitForShow(QuietBubbleElementId),
      CheckQuietPromptMessage(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_PREDICTION_SERVICE_DESCRIPTION),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(QuietBubbleElementId, "QuietPromptPopupBubble",
                        "5875965"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Geolocation1DenyRequestChipTest) {
  SetCannedUiDecision(
      Decision::UseQuietUi(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                           Decision::ShowNoWarning()));
  permissions::PermissionActionsHistory* permission_actions_history =
      GetPermissionActionsHistory();

  // 1 deny = Crowd Deny message.
  permission_actions_history->RecordAction(
      permissions::PermissionAction::DENIED,
      permissions::RequestType::kGeolocation,
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(QuietChipElementId),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      WaitForShow(QuietChipElementId), CheckChipIsRequest(true),
      CheckChipText(IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_TITLE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "RequestChip", "5875965"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Geolocation1DenyPromptTest) {
  SetCannedUiDecision(
      Decision::UseQuietUi(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                           Decision::ShowNoWarning()));
  permissions::PermissionActionsHistory* permission_actions_history =
      GetPermissionActionsHistory();

  // 1 deny = Crowd Deny message.
  permission_actions_history->RecordAction(
      permissions::PermissionAction::DENIED,
      permissions::RequestType::kGeolocation,
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(QuietChipElementId),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      WaitForShow(QuietChipElementId), CheckChipIsRequest(true),
      CheckChipText(IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_TITLE),
      EnsureNotPresent(QuietBubbleElementId), PressButton(QuietChipElementId),
      WaitForShow(QuietBubbleElementId),
      CheckQuietPromptMessage(
          IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_PREDICTION_SERVICE_DESCRIPTION),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(QuietBubbleElementId, "QuietPromptPopupBubble",
                        "5934206"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Geolocation5DeniesPromptTest) {
  SetCannedUiDecision(
      Decision::UseQuietUi(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                           Decision::ShowNoWarning()));
  permissions::PermissionActionsHistory* permission_actions_history =
      GetPermissionActionsHistory();

  // 5 denies = CPSS message.
  for (int i = 0; i < 5; i++) {
    permission_actions_history->RecordAction(
        permissions::PermissionAction::DENIED,
        permissions::RequestType::kGeolocation,
        permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
  }

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(QuietChipElementId),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      WaitForShow(QuietChipElementId), CheckChipIsRequest(true),
      CheckChipText(IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_TITLE),
      EnsureNotPresent(QuietBubbleElementId), PressButton(QuietChipElementId),
      WaitForShow(QuietBubbleElementId),
      CheckQuietPromptMessage(
          IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_PREDICTION_SERVICE_DESCRIPTION),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(QuietBubbleElementId, "QuietPromptPopupBubble",
                        "5875965"));
}

struct QuietPromptInfoBarTestCase {
  std::string test_name;
  std::string js;
  bool should_show_infobar;
};

class QuietPromptInteractiveParamUITest
    : public QuietPromptInteractiveUITest,
      public testing::WithParamInterface<QuietPromptInfoBarTestCase> {
 public:
  void SetUp() override {
    feature_list_->InitWithFeatures(
        {permissions::features::kPermissionPromiseLifetimeModulation},
        /*disabled_features=*/{});
    QuietPromptInteractiveUITest::SetUp();
  }

 private:
  std::unique_ptr<ScopedFeatureList> feature_list_ =
      std::make_unique<ScopedFeatureList>();
};

INSTANTIATE_TEST_SUITE_P(
    PermissionChangeListenerTests,
    QuietPromptInteractiveParamUITest,
    ValuesIn<QuietPromptInfoBarTestCase>({
        {"NotificationWithoutInfobar",
         "() => {"
         " registerDummyPermissionChangeListener(\"notifications\")"
         " .then(()=> {"
         "   requestNotification();"
         "  });"
         "}",
         false},
        {"NotificationWithInfobar", "requestNotification", true},
        {"LocationWithoutInfobar",
         "() => {"
         " registerDummyPermissionChangeListener(\"geolocation\")"
         " .then(()=> {"
         "   requestLocation();"
         "  });"
         "}",
         false},
        {"GeolocationWithInfobar", "requestLocation", true},
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        QuietPromptInteractiveParamUITest::ParamType>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(QuietPromptInteractiveParamUITest,
                       AllowQuietPromptAndMaybeShowInfobar) {
  SetCannedUiDecision(
      Decision::UseQuietUi(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                           Decision::ShowNoWarning()));

  auto [test_name, js, should_show] = GetParam();

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ExecuteJs(kWebContentsElementId, js), WaitForShow(QuietChipElementId),
      CheckChipIsRequest(true), PressButton(QuietChipElementId),
      WaitForShow(QuietBubbleElementId), PressButton(QuietBubbleAllowElementId),
      WaitForHide(QuietBubbleElementId), WaitForShow(QuietChipElementId),
      WaitForHide(QuietChipElementId),
      If([should_show]() { return should_show; },
         Then(WaitForShow(InfobarElementId)),
         Else(EnsureNotPresent(InfobarElementId))),
      Do([&] {
        histogram_tester_.ExpectBucketCount(
            "Permissions.QuietPrompt.Preignore.PageReloadInfoBar", should_show,
            1);
      }),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, test_name, "6768828"));
}
