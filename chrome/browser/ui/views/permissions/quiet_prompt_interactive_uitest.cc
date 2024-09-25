// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
const char kLocationBarView[] = "LocationBarView";

// Test implementation of PermissionUiSelector that always returns a canned
// decision.
class TestQuietNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      const Decision& canned_decision)
      : canned_decision_(canned_decision) {}
  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(canned_decision_);
  }

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override {
    return request_type == permissions::RequestType::kNotifications ||
           request_type == permissions::RequestType::kGeolocation;
  }

 private:
  Decision canned_decision_;
};
}  // namespace

class QuietPromptInteractiveUITest : public InteractiveBrowserTest {
 public:
  QuietPromptInteractiveUITest() {
    scoped_features_.InitAndEnableFeature(
        permissions::features::kPermissionPredictionsV3);
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~QuietPromptInteractiveUITest() override {}

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
    browser()->swap_location_bar_models(&location_bar_model);

    test_location_bar_model_->set_formatted_full_url(text);

    // Normally the URL for display has portions elided. We aren't doing that in
    // this case, because that is irrevelant for these tests.
    test_location_bar_model_->set_url_for_display(text);

    omnibox_view->Update();
  }

  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(std::optional<QuietUiReason> quiet_ui_reason,
                           std::optional<WarningReason> warning_reason) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

  // Checks that the permission chip is visible and in the given mode.
  // If `is_request` is false, should be in indicator mode instead.
  auto CheckChipIsRequest(bool is_request) {
    return CheckViewProperty(PermissionChipView::kElementIdForTesting,
                             &PermissionChipView::GetIsRequestForTesting,
                             is_request);
  }

  auto CheckChipText(int id_string) {
    return CheckViewProperty(PermissionChipView::kElementIdForTesting,
                             &PermissionChipView::GetText,
                             l10n_util::GetStringUTF16(id_string));
  }

  auto CheckQuietPromptMessage(int id_string) {
    return CheckViewProperty(
        ContentSettingBubbleContents::kMainElementId,
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

 private:
  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Notification1DenyRequestChipTest) {
  SetCannedUiDecision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                      std::nullopt);
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
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(true),
      CheckChipText(IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_TITLE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "RequestChip", "5875965"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Notification1DenyPromptTest) {
  SetCannedUiDecision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                      std::nullopt);
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
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(true),
      CheckChipText(IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_TITLE),
      EnsureNotPresent(ContentSettingBubbleContents::kMainElementId),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      CheckQuietPromptMessage(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CROWD_DENY_DESCRIPTION),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(ContentSettingBubbleContents::kMainElementId,
                        "QuietPromptPopupBubble", "5875965"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Notification5DeniesPromptTest) {
  SetCannedUiDecision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                      std::nullopt);
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
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(true),
      CheckChipText(IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_TITLE),
      EnsureNotPresent(ContentSettingBubbleContents::kMainElementId),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      CheckQuietPromptMessage(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_PREDICTION_SERVICE_DESCRIPTION),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(ContentSettingBubbleContents::kMainElementId,
                        "QuietPromptPopupBubble", "5875965"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Geolocation1DenyRequestChipTest) {
  SetCannedUiDecision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                      std::nullopt);
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
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(true),
      CheckChipText(IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_TITLE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "RequestChip", "5875965"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Geolocation1DenyPromptTest) {
  SetCannedUiDecision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                      std::nullopt);
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
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(true),
      CheckChipText(IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_TITLE),
      EnsureNotPresent(ContentSettingBubbleContents::kMainElementId),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      CheckQuietPromptMessage(
          IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_CROWD_DENY_DESCRIPTION),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(ContentSettingBubbleContents::kMainElementId,
                        "QuietPromptPopupBubble", "5875965"));
}

IN_PROC_BROWSER_TEST_F(QuietPromptInteractiveUITest,
                       CPSSv3Geolocation5DeniesPromptTest) {
  SetCannedUiDecision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                      std::nullopt);
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
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(true),
      CheckChipText(IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_TITLE),
      EnsureNotPresent(ContentSettingBubbleContents::kMainElementId),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      CheckQuietPromptMessage(
          IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_PREDICTION_SERVICE_DESCRIPTION),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(ContentSettingBubbleContents::kMainElementId,
                        "QuietPromptPopupBubble", "5875965"));
}
