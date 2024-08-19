// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
const char kFirstPermissionRow[] = "FirstPermissionRow";
const char kSecondPermissionRow[] = "SecondPermissionRow";

}  // namespace

class DashboardKombuchaInteractiveUITest : public InteractiveBrowserTest {
 public:
  DashboardKombuchaInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    feature_list_.InitWithFeatures(
        {content_settings::features::kLeftHandSideActivityIndicators
#if BUILDFLAG(IS_CHROMEOS)
         ,
         content_settings::features::kCrosSystemLevelPermissionBlockedWarnings
#endif
        },
        {});
  }

  ~DashboardKombuchaInteractiveUITest() override = default;
  DashboardKombuchaInteractiveUITest(
      const DashboardKombuchaInteractiveUITest&) = delete;
  void operator=(const DashboardKombuchaInteractiveUITest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
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

  auto CheckChipText(int id_string) {
    return CheckViewProperty(PermissionChipView::kElementIdForTesting,
                             &PermissionChipView::GetText,
                             l10n_util::GetStringUTF16(id_string));
  }

  void SetPermission(ContentSettingsType type, ContentSetting setting) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());

    map->SetContentSettingDefaultScope(GetURL(), GetURL(), type, setting);
  }

  StateChange GetCameraStreamStateChange() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMediaStreamOpenEvent);
    StateChange state_change;
    state_change.test_function = "(_) => typeof cameraStream !== 'undefined'";
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kMediaStreamOpenEvent;
    return state_change;
  }

  StateChange GetMicStreamStateChange() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMediaStreamOpenEvent);
    StateChange state_change;
    state_change.test_function = "(_) => typeof micStream !== 'undefined'";
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kMediaStreamOpenEvent;
    return state_change;
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
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

// 1. Enable Camera permission
// 2. Use `getUserMedia` to show camera activity indicator
// 3. Click on the indicator to open PageInfo
// 4. Verify that Camera permission is shown in PageInfo
// 5. Verify that Camera permission has "Using now" subtitle.
// 6. Verify that the system settings link is not shown.
IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest, CameraUsingTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_IN_USE),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Camera
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA)),
      WaitForShow(PermissionToggleRowView::kRowSubTitleCameraElementId),
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetRowSubTitleForTesting,
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_USING_NOW)),
      EnsureNotPresent(
          PermissionToggleRowView::kPermissionDisabledAtSystemLevelElementId));
}

// 1. Enable Camera permission
// 2. Use `getUserMedia` to show camera activity indicator
// 3. Click on the indicator to open PageInfo
// 4. Verify that Camera permission is shown in PageInfo
// 5. Verify that Camera permission has "Using now" subtitle.
// 6. Verify that the system settings link is shown.
IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       CameraUsingTestWithSystemBlock) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  system_permission_settings::ScopedSettingsForTesting scoped_system_permission(
      ContentSettingsType::MEDIASTREAM_CAMERA, true);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_IN_USE),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Camera
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA)),
      WaitForShow(PermissionToggleRowView::kRowSubTitleCameraElementId),
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetRowSubTitleForTesting,
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_USING_NOW)),
      WaitForShow(
          PermissionToggleRowView::kPermissionDisabledAtSystemLevelElementId));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       MicrophoneUsingTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_MICROPHONE_IN_USE),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Mic
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowTitleForTesting,
                        l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_MIC)),
      WaitForShow(PermissionToggleRowView::kRowSubTitleMicrophoneElementId),
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetRowSubTitleForTesting,
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_USING_NOW)),
      EnsureNotPresent(
          PermissionToggleRowView::kPermissionDisabledAtSystemLevelElementId)

  );
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       MicrophoneUsingTestWithSystemBlock) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW);

  system_permission_settings::ScopedSettingsForTesting scoped_system_permission(
      ContentSettingsType::MEDIASTREAM_MIC, true);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_MICROPHONE_IN_USE),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Mic
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowTitleForTesting,
                        l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_MIC)),
      WaitForShow(PermissionToggleRowView::kRowSubTitleMicrophoneElementId),
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetRowSubTitleForTesting,
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_USING_NOW)),
      WaitForShow(
          PermissionToggleRowView::kPermissionDisabledAtSystemLevelElementId)

  );
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest, LocationUsingTest) {
  SetPermission(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  system_permission_settings::ScopedSettingsForTesting scoped_system_permission(
      ContentSettingsType::GEOLOCATION, false);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      // Request chip should be hidden.
      PressButton(kLocationIconElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Location
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_LOCATION)),
      EnsureNotPresent(
          PermissionToggleRowView::kPermissionDisabledAtSystemLevelElementId));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       LocationUsingTestWithSystemBlock) {
  SetPermission(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  system_permission_settings::ScopedSettingsForTesting scoped_system_permission(
      ContentSettingsType::GEOLOCATION, true);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      // Request chip should be hidden.
      PressButton(kLocationIconElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Location
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_LOCATION)),
      WaitForShow(
          PermissionToggleRowView::kPermissionDisabledAtSystemLevelElementId));
}
#endif

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       CameraAndMicrophoneUsingTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestCameraAndMicrophone"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_MICROPHONE_CAMERA_IN_USE),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        2),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Camera
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA)),
      WaitForShow(PermissionToggleRowView::kRowSubTitleCameraElementId),
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kSecondPermissionRow, 1u),
      // Verify the row label is Mic
      CheckViewProperty(kSecondPermissionRow,
                        &PermissionToggleRowView::GetRowTitleForTesting,
                        l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_MIC)),
      CheckViewProperty(
          kSecondPermissionRow,
          &PermissionToggleRowView::GetRowSubTitleForTesting,
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_USING_NOW))

  );
}

// 1. Enable Camera permission
// 2. Use `getUserMedia` to show camera activity indicator
// 3. Click on the indicator to open PageInfo
// 4. Verify that Camera permission is shown in PageInfo
// 5. Verify that Camera permission has no subtitle.
IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       CameraPermissionBlockedInUseTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_BLOCK);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_NOT_ALLOWED),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Camera.
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA)),
      EnsureNotPresent(PermissionToggleRowView::kRowSubTitleCameraElementId));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       MicrophonePermissionBlockedInUseTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_BLOCK);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_MICROPHONE_NOT_ALLOWED),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Camera
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowTitleForTesting,
                        l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_MIC)),
      EnsureNotPresent(PermissionToggleRowView::kRowSubTitleCameraElementId));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       CameraAndMicrophonePermissionsBlockedInUseTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_BLOCK);
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_BLOCK);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestCameraAndMicrophone"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false),
      CheckChipText(IDS_MICROPHONE_CAMERA_NOT_ALLOWED),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        2),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Camera
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA)),
      EnsureNotPresent(PermissionToggleRowView::kRowSubTitleCameraElementId),
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kSecondPermissionRow, 1u),
      // Verify the row label is Mic
      CheckViewProperty(kSecondPermissionRow,
                        &PermissionToggleRowView::GetRowTitleForTesting,
                        l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_MIC)),
      EnsureNotPresent(
          PermissionToggleRowView::kRowSubTitleMicrophoneElementId));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       CameraAllowStartStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForStateChange(kWebContentsElementId, GetCameraStreamStateChange()),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_IN_USE),
      ExecuteJs(kWebContentsElementId, "stopCamera"),
      WaitForHide(PermissionChipView::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       CameraBlockStartStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_BLOCK);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_NOT_ALLOWED),
      // Blocked indicator disappears by itself after a short delay.
      WaitForHide(PermissionChipView::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       MicrophoneAllowStartStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForStateChange(kWebContentsElementId, GetMicStreamStateChange()),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      CheckChipText(IDS_MICROPHONE_IN_USE),
      ExecuteJs(kWebContentsElementId, "stopMic"),
      WaitForHide(PermissionChipView::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       MicBlockStartStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_BLOCK);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      // Blocked indicator disappears by itself after a short delay.
      WaitForHide(PermissionChipView::kElementIdForTesting));
}

// Make sure PageInfo does not re-open on an indicator click.
IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       SuppressPageInfoReopen) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kElementIdForTesting),
      // Clicking on LHS indicator opens PageInfo, the second click should hide
      // PageInfo.
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForHide(PageInfoMainView::kPermissionsElementId),
      // Repeat again to make sure all flags are reset and can be reused.
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForHide(PageInfoMainView::kPermissionsElementId));
}
