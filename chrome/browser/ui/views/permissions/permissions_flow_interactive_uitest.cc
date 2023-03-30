// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_row_view.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
const char kFirstPermissionRow[] = "FirstPermissionRow";

}  // namespace

class PermissionsFlowInteractiveUITest : public InteractiveBrowserTest {
 public:
  PermissionsFlowInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~PermissionsFlowInteractiveUITest() override = default;
  PermissionsFlowInteractiveUITest(const PermissionsFlowInteractiveUITest&) =
      delete;
  void operator=(const PermissionsFlowInteractiveUITest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    // set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

  // Navigates a tab to `GetURL()` and opens PageInfo.
  auto NavigateAndOpenPageInfo() {
    return Steps(InstrumentTab(kWebContentsElementId),
                 NavigateWebContents(kWebContentsElementId, GetURL()),
                 PressButton(kLocationIconElementId),
                 WaitForShow(kPageInfoElementId));
  }

 protected:
  virtual std::string GetTestPageRelativeURL() {
    return "/permissions/requests.html";
  }

  GURL GetURL() {
    return https_server()->GetURL("a.test", GetTestPageRelativeURL());
  }

  void SetPermission(ContentSettingsType type, ContentSetting setting) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());

    map->SetContentSettingDefaultScope(GetURL(), GetURL(), type, setting);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Tests that by default PageInfo has no visible permission.
IN_PROC_BROWSER_TEST_F(PermissionsFlowInteractiveUITest,
                       PageInfoWithEmptyPermissionsTest) {
  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      // There are no permissions in PageInfo as all of them have default state.
      CheckViewProperty(kPageInfoElementId,
                        &PageInfoMainView::GetVisiblePermissionsForTesting, 0));
}

IN_PROC_BROWSER_TEST_F(PermissionsFlowInteractiveUITest,
                       PageInfoCameraPermissionsTest) {
  // Set Camera permission to Allow so it becomes visible in PageInfo.
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      CheckViewProperty(kPageInfoElementId,
                        &PageInfoMainView::GetVisiblePermissionsForTesting, 1),
      // A view with permissions in PageInfo
      WaitForShow(kPageInfoPermissionsElementId),
      // Set id to the first children of `kPageInfoPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(kPageInfoPermissionsElementId, kFirstPermissionRow, 0),
      // Verify the row label is Camera
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA)));
}

IN_PROC_BROWSER_TEST_F(PermissionsFlowInteractiveUITest,
                       NotificationsPermissionRequestTest) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // Request permission.
      ExecuteJs(kWebContentsElementId, "requestNotification()"),
      WaitForShow(kPermissionRequestChipElementId),
      WaitForShow(kPermissionPromptBubbleElementId),
      WaitForShow(kPermissionPromptAllowButtonElementId),
      // Permission prompt bubble is shown, click on the Allow button.
      PressButton(kPermissionPromptAllowButtonElementId),
      WaitForHide(kPermissionPromptBubbleElementId),
      // Click on the PageInfo icon and verify that the first permission is
      // Notification.
      PressButton(kLocationIconElementId), WaitForShow(kPageInfoElementId),
      WaitForShow(kPageInfoPermissionsElementId),
      NameChildView(kPageInfoPermissionsElementId, kFirstPermissionRow, 0),
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS)));
}
