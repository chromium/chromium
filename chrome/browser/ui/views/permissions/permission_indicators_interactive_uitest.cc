// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

}  // namespace

class PermissionIndicatorsInteractiveUITest : public InteractiveBrowserTest {
 public:
  PermissionIndicatorsInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~PermissionIndicatorsInteractiveUITest() override = default;
  PermissionIndicatorsInteractiveUITest(
      const PermissionIndicatorsInteractiveUITest&) = delete;
  void operator=(const PermissionIndicatorsInteractiveUITest&) = delete;

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

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  void SetPermission(ContentSettingsType type, ContentSetting setting) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());

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

  // Waits for the media stream (camera/mic) activity indicator to show and
  // verifies its icon.
  auto VerifyMediaStreamIndicator(const gfx::VectorIcon* expected_icon) {
    return AfterShow(
        PermissionChipView::kIndicatorChipElementId,
        base::BindLambdaForTesting([this,
                                    expected_icon](ui::TrackedElement* element) {
          auto* dashboard_controller = BrowserWindow::FromBrowser(browser())
                                           ->GetLocationBar()
                                           ->GetPermissionDashboardController();
          ASSERT_TRUE(dashboard_controller);
          auto* model = dashboard_controller->indicator_model_for_testing();
          ASSERT_TRUE(model);
          EXPECT_EQ(model->icon(), expected_icon);
          // Permission is granted, there is no badge.
          EXPECT_EQ(model->get_icon_badge(), &gfx::VectorIcon::EmptyIcon());
        }));
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Tests that after a camera stream is stopped, a camera activity indicator is
// hidden.
IN_PROC_BROWSER_TEST_F(PermissionIndicatorsInteractiveUITest,
                       CameraAccessAndStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      // `getUserMedia` is async, so wait until media stream is opened.
      WaitForStateChange(kWebContentsElementId, GetCameraStreamStateChange()),
      VerifyMediaStreamIndicator(
          &(features::IsRoundedIconsEnabled()
                ? vector_icons::kVideocamIcon
                : vector_icons::kVideocamChromeRefreshOldIcon)),
      ExecuteJs(kWebContentsElementId, "stopCamera"),
      WaitForHide(PermissionChipView::kIndicatorChipElementId));
}

// Start using a camera, then start using a microphone, stop using the camera,
// stop using the microphone and check that no indicator is visible.
IN_PROC_BROWSER_TEST_F(PermissionIndicatorsInteractiveUITest,
                       CameraAndMicAccessAndStopTest) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ExecuteJs(kWebContentsElementId, "requestMicrophone"),
      WaitForStateChange(kWebContentsElementId, GetMicStreamStateChange()),
      VerifyMediaStreamIndicator(
          &(features::IsRoundedIconsEnabled()
                ? vector_icons::kMicIcon
                : vector_icons::kMicChromeRefreshOldIcon)),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      // `getUserMedia` is async, so wait until media stream is opened.
      WaitForStateChange(kWebContentsElementId, GetCameraStreamStateChange()),
      VerifyMediaStreamIndicator(
          &(features::IsRoundedIconsEnabled()
                ? vector_icons::kVideocamIcon
                : vector_icons::kVideocamChromeRefreshOldIcon)),
      ExecuteJs(kWebContentsElementId, "stopCamera"),
      ExecuteJs(kWebContentsElementId, "stopMic"),
      WaitForHide(PermissionChipView::kIndicatorChipElementId));
}
