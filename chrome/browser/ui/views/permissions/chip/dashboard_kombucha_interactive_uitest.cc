// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
const char kFirstPermissionRow[] = "FirstPermissionRow";
const char kSecondPermissionRow[] = "SecondPermissionRow";
const char kLocationBarView[] = "LocationBarView";

}  // namespace

class DashboardKombuchaInteractiveUITest : public InteractiveBrowserTest {
 public:
  DashboardKombuchaInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    feature_list_.InitWithFeatures(
        {content_settings::features::kLeftHandSideActivityIndicators}, {});
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

  // Set static site name to prevent flakes caused by changing port.
  void SetStaticSiteName(std::u16string site_name) {
    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(bubble_view->presenter_for_testing()->GetSubjectNameForDisplay(),
              site_name);
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

  auto CheckChipText(int id_string) {
    return CheckViewProperty(PermissionChipView::kIndicatorChipElementId,
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

  LocationBarView* GetLocationBarView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

  PermissionDashboardController* GetDashboardController() {
    return GetLocationBarView()->permission_dashboard_controller();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<LocationBarModel> original_location_bar_model_;
};

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       PermissionChipClickTest) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()), Do([this]() {
        GetLocationBarView()->GetChipController()->DoNotCollapseForTesting();
      }),
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      // Make sure the request chip is visible.
      WaitForShow(PermissionChipView::kPermissionRequestChipElementId),
      CheckChipIsRequest(true),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "PermissionChipClickTest_RequestChip",
                 "7663434"),
      // Make sure the permission popup bubble is visible.
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PermissionPromptBubbleBaseView::kMainViewId,
                 "PermissionChipClickTest_PromptBubble", "7663434"),
      PressButton(PermissionChipView::kPermissionRequestChipElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      // The permission chip is hidden because the permission
      // request was dismissed instantly after a click.
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId));
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_IN_USE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "CameraUsingTest_IndicatorChip", "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "CameraUsingTest_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_IN_USE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView,
                 "CameraUsingTestWithSystemBlock_IndicatorChip", "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "CameraUsingTestWithSystemBlock_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_MICROPHONE_IN_USE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "MicrophoneUsingTest_IndicatorChip",
                 "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "MicrophoneUsingTest_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_MICROPHONE_IN_USE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView,
                 "MicrophoneUsingTestWithSystemBlock_IndicatorChip", "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "MicrophoneUsingTestWithSystemBlock_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      // Request chip should be hidden.
      PressButton(kLocationIconElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "LocationUsingTest_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      ExecuteJs(kWebContentsElementId, "requestLocation"),
      // Request chip should be hidden.
      PressButton(kLocationIconElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "LocationUsingTestWithSystemBlock_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestCameraAndMicrophone"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_MICROPHONE_CAMERA_IN_USE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "CameraAndMicrophoneUsingTest_IndicatorChip",
                 "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "CameraAndMicrophoneUsingTest_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_NOT_ALLOWED),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView,
                 "CameraPermissionBlockedInUseTest_IndicatorChip", "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "CameraPermissionBlockedInUseTest_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_MICROPHONE_NOT_ALLOWED),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView,
                 "MicrophonePermissionBlockedInUseTest_IndicatorChip",
                 "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "MicrophonePermissionBlockedInUseTest_PageInfo", "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestCameraAndMicrophone"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false),
      CheckChipText(IDS_MICROPHONE_CAMERA_NOT_ALLOWED),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView,
                 "CameraAndMicrophonePermissionsBlockedInUseTest_IndicatorChip",
                 "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "CameraAndMicrophonePermissionsBlockedInUseTest_PageInfo",
                 "7663434"),
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
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForStateChange(kWebContentsElementId, GetCameraStreamStateChange()),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_IN_USE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "CameraAllowStartStopTest_IndicatorChip",
                 "7663434"),
      ExecuteJs(kWebContentsElementId, "stopCamera"),
      WaitForHide(PermissionChipView::kIndicatorChipElementId));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       CameraBlockStartStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_BLOCK);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipIsRequest(false), CheckChipText(IDS_CAMERA_NOT_ALLOWED),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "CameraBlockStartStopTest_IndicatorChip",
                 "7663434"),
      // Blocked indicator disappears by itself after a short delay, but
      // DoNotCollapseForTesting() prevents it. Thus, hide it manually.
      Do([this]() { GetDashboardController()->HideIndicatorsForTesting(); }),
      WaitForHide(PermissionChipView::kIndicatorChipElementId));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       MicrophoneAllowStartStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForStateChange(kWebContentsElementId, GetMicStreamStateChange()),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      CheckChipText(IDS_MICROPHONE_IN_USE),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "MicrophoneAllowStartStopTest_IndicatorChip",
                 "7663434"),
      ExecuteJs(kWebContentsElementId, "stopMic"),
      WaitForHide(PermissionChipView::kIndicatorChipElementId));
}

IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       MicBlockStartStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_BLOCK);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionDashboardView::kDashboardElementId),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "MicBlockStartStopTest_IndicatorChip",
                 "7663434"),
      // Blocked indicator disappears by itself after a short delay, but
      // DoNotCollapseForTesting() prevents it. Thus, hide it manually.
      Do([this]() { GetDashboardController()->HideIndicatorsForTesting(); }),
      WaitForHide(PermissionChipView::kIndicatorChipElementId));
}

// Make sure PageInfo does not re-open on an indicator click.
IN_PROC_BROWSER_TEST_F(DashboardKombuchaInteractiveUITest,
                       SuppressPageInfoReopen) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      Do([this]() { GetDashboardController()->DoNotCollapseForTesting(); }),
      EnsureNotPresent(PermissionChipView::kIndicatorChipElementId),
      EnsureNotPresent(PermissionChipView::kPermissionRequestChipElementId),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      WaitForShow(PermissionChipView::kIndicatorChipElementId),
      NameView(kLocationBarView, GetLocationBarView()),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(kLocationBarView, "SuppressPageInfoReopen_IndicatorChip",
                 "7663434"),
      // Clicking on LHS indicator opens PageInfo, the second click should hide
      // PageInfo.
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      Do([this]() { SetStaticSiteName(u"test.com"); }),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(PageInfoMainView::kMainLayoutElementId,
                 "SuppressPageInfoReopen_PageInfo", "7663434"),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForHide(PageInfoMainView::kPermissionsElementId),
      // Repeat again to make sure all flags are reset and can be reused.
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      PressButton(PermissionChipView::kIndicatorChipElementId),
      WaitForHide(PageInfoMainView::kPermissionsElementId));
}
