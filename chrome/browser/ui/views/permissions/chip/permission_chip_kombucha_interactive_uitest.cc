// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/permissions/test/mock_permission_ui_selector.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
const char kLocationBarView[] = "LocationBarView";
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

    // Override url in the omnibox to avoid test flakiness due to different port
    // in the original url.
    std::u16string url_override(u"https://www.test.com/");
    OverrideVisibleUrlInLocationBar(url_override);
  }

  void TearDownOnMainThread() override {
    // Restore the original LocationBarModel if it was overridden.
    if (original_location_bar_model_) {
      browser()->GetFeatures().swap_location_bar_models(
          &original_location_bar_model_);
    }
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set a window's size to avoid pixel tests flakiness due to different
    // widths of the omnibox.
    command_line->AppendSwitchASCII(switches::kWindowSize,
                                    base::StringPrintf("%d,%d", 800, 600));
    InteractiveBrowserTest::SetUpCommandLine(command_line);
  }

  void OverrideVisibleUrlInLocationBar(const std::u16string& text) {
    OmniboxView* omnibox_view = GetLocationBarView()->GetOmniboxView();

    // The pixel tests are sensitive to the URL displayed in the omnibox, as the
    // port number of the test server varies. To prevent flakiness, we override
    // the LocationBarModel with a TestLocationBarModel that returns a static
    // URL. We preserve the original model to restore it during teardown.
    auto test_location_bar_model = std::make_unique<TestLocationBarModel>();
    test_location_bar_model->set_formatted_full_url(text);
    test_location_bar_model->set_url_for_display(text);

    std::unique_ptr<LocationBarModel> new_model_for_swap =
        std::move(test_location_bar_model);
    browser()->GetFeatures().swap_location_bar_models(&new_model_for_swap);
    original_location_bar_model_ = std::move(new_model_for_swap);

    omnibox_view->Update();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  // Checks that the permission chip is visible and in the given mode.
  // If `is_request` is false, should be in indicator mode instead.
  auto CheckChipIsRequest(bool is_request) {
    return CheckViewProperty(
        is_request ? PermissionChipView::kPermissionRequestChipElementId
                   : PermissionChipView::kIndicatorChipElementId,
        &PermissionChipView::GetIsRequestForTesting, is_request);
  }

  LocationBarView* GetLocationBarView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;
  using Decision = permissions::PermissionUiSelector::Decision;

  void SetCannedUiDecision(const Decision& decision) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<MockPermissionUiSelector>(decision));
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
  std::unique_ptr<LocationBarModel> original_location_bar_model_;
};

// Tests that after a click on the permission chip a permission request will be
// dismissed and the chip will be hidden.
IN_PROC_BROWSER_TEST_F(PermissionChipKombuchaInteractiveUITest,
                       PermissionChipClickTest) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      // Make sure the request chip is visible.
      WaitForShow(PermissionChipView::kPermissionRequestChipElementId),
      CheckChipIsRequest(true),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "NotificationsRequestChip", "7633407"),
      // Make sure the permission popup bubble is visible.
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      PressButton(PermissionChipView::kPermissionRequestChipElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      // The permission chip is hidden because the permission
      // request was dismissed instantly after a click.
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId));
}

// Tests that after the second click on the quiet permission chip a permission
// request will be dismissed and the chip will be hidden.
IN_PROC_BROWSER_TEST_F(PermissionChipKombuchaInteractiveUITest,
                       QuietPermissionChipClickTest) {
  SetCannedUiDecision(Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                           Decision::ShowNoWarning()));

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      // Make sure the request chip is visible.
      WaitForShow(PermissionChipView::kPermissionRequestChipElementId),
      CheckChipIsRequest(true),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "QuietNotificationsRequestChip", "7633407"),
      // There is no auto-popup bubble for the quiet chip.
      EnsureNotPresent(ContentSettingBubbleContents::kMainElementId),
      // The first click - open a permission prompt popup bubble.
      PressButton(PermissionChipView::kPermissionRequestChipElementId),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      // The second click - hide the permission prompt popup bubble and dismiss
      // a permission request.
      PressButton(PermissionChipView::kPermissionRequestChipElementId),
      WaitForHide(ContentSettingBubbleContents::kMainElementId),
      // The permission chip is hidden because the permission request was
      // dismissed instantly after a click.
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId));
}

// Tests that after the second click on the quietest permission chip a
// permission request will be dismissed and the chip will be hidden.
IN_PROC_BROWSER_TEST_F(PermissionChipKombuchaInteractiveUITest,
                       QuietestPermissionChipClickTest) {
  SetCannedUiDecision(Decision::UseQuietUi(
      QuietUiReason::kTriggeredDueToAbusiveContent, Decision::ShowNoWarning()));

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      // Make sure the request chip is visible.
      WaitForShow(PermissionChipView::kPermissionRequestChipElementId),
      CheckChipIsRequest(true),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "QuietestNotificationsRequestChip",
                 "7633407"),
      // There is no auto-popup bubble for the quiet chip.
      EnsureNotPresent(ContentSettingBubbleContents::kMainElementId),
      // The first click - open a permission prompt popup bubble.
      PressButton(PermissionChipView::kPermissionRequestChipElementId),
      WaitForShow(ContentSettingBubbleContents::kMainElementId),
      // The second click - hide the permission prompt popup bubble and dismiss
      // a permission request.
      PressButton(PermissionChipView::kPermissionRequestChipElementId),
      WaitForHide(ContentSettingBubbleContents::kMainElementId),
      // The permission chip is hidden because the permission request was
      // dismissed instantly after a click.
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId));
}
