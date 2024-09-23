// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

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
    return request_type == permissions::RequestType::kNotifications;
  }

 private:
  Decision canned_decision_;
};

}  // namespace

class PermissionChipKombuchaInteractiveUITest : public InteractiveBrowserTest {
 public:
  PermissionChipKombuchaInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~PermissionChipKombuchaInteractiveUITest() override = default;
  PermissionChipKombuchaInteractiveUITest(
    const PermissionChipKombuchaInteractiveUITest&) = delete;
  void operator=(const PermissionChipKombuchaInteractiveUITest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();

    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  // Checks that the permission chip is visible and in the given mode.
  // If `is_request` is false, should be in indicator mode instead.
  auto CheckChipIsRequest(bool is_request) {
    return CheckViewProperty(PermissionChipView::kElementIdForTesting,
                             &PermissionChipView::GetIsRequestForTesting,
                             is_request);
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(std::optional<QuietUiReason> quiet_ui_reason,
                           std::optional<WarningReason> warning_reason) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

// Tests that after a click on the permission chip a permission request will be
// dismissed and the chip will be hidden.
IN_PROC_BROWSER_TEST_F(PermissionChipKombuchaInteractiveUITest,
                       PermissionChipClickTest) {
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()),
                  ExecuteJs(kWebContentsElementId, "requestNotification"),
                  // Make sure the request chip is visible.
                  WaitForShow(PermissionChipView::kElementIdForTesting),
                  CheckChipIsRequest(true),
                  // Make sure the permission popup bubble is visible.
                  WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
                  PressButton(PermissionChipView::kElementIdForTesting),
                  WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
                  // The permission chip is hidden because the permission
                  // request was dismissed instantly after a click.
                  EnsureNotPresent(PermissionChipView::kElementIdForTesting));
}

// Tests that after the second click on the quiet permission chip a permission
// request will be dismissed and the chip will be hidden.
IN_PROC_BROWSER_TEST_F(PermissionChipKombuchaInteractiveUITest,
                       QuietPermissionChipClickTest) {
  SetCannedUiDecision(QuietUiReason::kEnabledInPrefs, std::nullopt);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      // Make sure the request chip is visible.
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(true),
      // There is no auto-popup bubble for the quiet chip.
      EnsureNotPresent(ContentSettingBubbleContents::kMainElementId),
      // The first click - open a permission prompt popup bubble.
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      // The second click - hide the permission prompt popup bubble and dismiss
      // a permission request.
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForHide(ContentSettingBubbleContents::kMainElementId),
      // The permission chip is hidden because the permission request was
      // dismissed instantly after a click.
      EnsureNotPresent(PermissionChipView::kElementIdForTesting));
}

// Tests that after the second click on the quietest permission chip a
// permission request will be dismissed and the chip will be hidden.
IN_PROC_BROWSER_TEST_F(PermissionChipKombuchaInteractiveUITest,
                       QuietestPermissionChipClickTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      std::nullopt);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      // Make sure the request chip is visible.
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(true),
      // There is no auto-popup bubble for the quiet chip.
      EnsureNotPresent(ContentSettingBubbleContents::kMainElementId),
      // The first click - open a permission prompt popup bubble.
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      // The second click - hide the permission prompt popup bubble and dismiss
      // a permission request.
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForHide(ContentSettingBubbleContents::kMainElementId),
      // The permission chip is hidden because the permission request was
      // dismissed instantly after a click.
      EnsureNotPresent(PermissionChipView::kElementIdForTesting));
}
