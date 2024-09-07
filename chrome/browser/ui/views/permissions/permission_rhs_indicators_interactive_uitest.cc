// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
const char kFirstPermissionRow[] = "FirstPermissionRow";

}  // namespace

class PermissionRHSIndicatorsInteractiveUITest : public InteractiveBrowserTest {
 public:
  PermissionRHSIndicatorsInteractiveUITest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFileSystemAccessPersistentPermissions},
        // This class is for RHS indicators.
        // LHS indicators feature should be disable.
        {content_settings::features::kLeftHandSideActivityIndicators});

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~PermissionRHSIndicatorsInteractiveUITest() override = default;
  PermissionRHSIndicatorsInteractiveUITest(
    const PermissionRHSIndicatorsInteractiveUITest&) = delete;
  void operator=(const PermissionRHSIndicatorsInteractiveUITest&) = delete;

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
                 PressButton(kLocationIconElementId));
  }

 protected:
  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  void SetPermission(ContentSettingsType type, ContentSetting setting) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());

    map->SetContentSettingDefaultScope(GetURL(), GetURL(), type, setting);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that by default PageInfo has no visible permission.
IN_PROC_BROWSER_TEST_F(PermissionRHSIndicatorsInteractiveUITest,
                       PageInfoWithEmptyPermissionsTest) {
  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      // There are no permissions in PageInfo as all of them have default state.
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        0));
}

IN_PROC_BROWSER_TEST_F(PermissionRHSIndicatorsInteractiveUITest,
                       PageInfoCameraPermissionsTest) {
  // Set Camera permission to Allow so it becomes visible in PageInfo.
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // A view with permissions in PageInfo
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Camera
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA)));
}

IN_PROC_BROWSER_TEST_F(PermissionRHSIndicatorsInteractiveUITest,
                       FileSystemPermissionsTest) {
  // Set File System permission to Allow so that it becomes visible in PageInfo.
  SetPermission(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                CONTENT_SETTING_ALLOW);

  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // A view with permissions in PageInfo.
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is File System.
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowTitleForTesting,
                        l10n_util::GetStringUTF16(
                            IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE)));
}

// The test requests Notifications permission, clicks Allow on a permission
// prompt, and verifies that a new entry for Notifications appeared in PageInfo.
IN_PROC_BROWSER_TEST_F(PermissionRHSIndicatorsInteractiveUITest,
                       NotificationsPermissionRequestTest) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // Request permission.
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      WaitForShow(PermissionPromptBubbleBaseView::kAllowButtonElementId),

      // Permission prompt bubble is shown, click on the Allow button.
      PressButton(PermissionPromptBubbleBaseView::kAllowButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      // Click on the PageInfo icon and verify that the first permission is
      // Notification.
      PressButton(kLocationIconElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS)));
}

IN_PROC_BROWSER_TEST_F(PermissionRHSIndicatorsInteractiveUITest,
                       CameraPermissionRequestTest) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // Request permission.
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      WaitForShow(PermissionPromptBubbleBaseView::kAllowButtonElementId),

      // Permission prompt bubble is shown, click on the Allow button.
      PressButton(PermissionPromptBubbleBaseView::kAllowButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      // Click on the PageInfo icon and verify that the first permission is
      // Notification.
      PressButton(kLocationIconElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA)));
}

IN_PROC_BROWSER_TEST_F(PermissionRHSIndicatorsInteractiveUITest,
                       CameraActivityIndicatorTest) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // Request permission.
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      WaitForShow(PermissionPromptBubbleBaseView::kAllowButtonElementId),

      // Permission prompt bubble is shown, click on the Allow button.
      PressButton(PermissionPromptBubbleBaseView::kAllowButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      WaitForShow(ContentSettingImageView::kMediaActivityIndicatorElementId),
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_for_testing,
          &vector_icons::kVideocamChromeRefreshIcon));
}

IN_PROC_BROWSER_TEST_F(PermissionRHSIndicatorsInteractiveUITest,
                       MicrophoneActivityIndicatorTest) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // Request permission.
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      WaitForShow(PermissionPromptBubbleBaseView::kAllowButtonElementId),

      // Permission prompt bubble is shown, click on the Allow button.
      PressButton(PermissionPromptBubbleBaseView::kAllowButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      WaitForShow(ContentSettingImageView::kMediaActivityIndicatorElementId),
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_for_testing,
          &vector_icons::kMicChromeRefreshIcon));
}

IN_PROC_BROWSER_TEST_F(PermissionRHSIndicatorsInteractiveUITest,
                       CameraAndMicrophoneActivityIndicatorTest) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // Request permission.
      ExecuteJs(kWebContentsElementId, "requestCameraAndMicrophone"),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      WaitForShow(PermissionPromptBubbleBaseView::kAllowButtonElementId),

      // Permission prompt bubble is shown, click on the Allow button.
      PressButton(PermissionPromptBubbleBaseView::kAllowButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      WaitForShow(ContentSettingImageView::kMediaActivityIndicatorElementId),
      // In case both camera and microphone permissions are requested and used
      // at once, we show a single indicator with a camera icon.
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_for_testing,
          &vector_icons::kVideocamChromeRefreshIcon));
}
