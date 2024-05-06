// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

}  // namespace

class PermissionIndicatorsInteractiveUITest : public InteractiveBrowserTest {
 public:
  PermissionIndicatorsInteractiveUITest() {
    scoped_feature_list_.InitWithFeatures(
        {content_settings::features::kImprovedSemanticsActivityIndicators},
        {content_settings::features::kLeftHandSideActivityIndicators});
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

  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
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
  base::test::ScopedFeatureList scoped_feature_list_;
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
      WaitForShow(ContentSettingImageView::kMediaActivityIndicatorElementId),
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_for_testing,
          &vector_icons::kVideocamChromeRefreshIcon),
      // Permission is granted, there is no badge.
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_badge_for_testing,
          &gfx::kNoneIcon),
      ExecuteJs(kWebContentsElementId, "stopCamera"),
      WaitForHide(ContentSettingImageView::kMediaActivityIndicatorElementId));
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
      WaitForShow(ContentSettingImageView::kMediaActivityIndicatorElementId),
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_for_testing,
          &vector_icons::kMicChromeRefreshIcon),
      // Permission is granted, there is no badge.
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_badge_for_testing,
          &gfx::kNoneIcon),
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      // `getUserMedia` is async, so wait until media stream is opened.
      WaitForStateChange(kWebContentsElementId, GetCameraStreamStateChange()),
      WaitForShow(ContentSettingImageView::kMediaActivityIndicatorElementId),
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_for_testing,
          &vector_icons::kVideocamChromeRefreshIcon),
      // Permission is granted, there is no badge.
      CheckViewProperty(
          ContentSettingImageView::kMediaActivityIndicatorElementId,
          &ContentSettingImageView::get_icon_badge_for_testing,
          &gfx::kNoneIcon),
      ExecuteJs(kWebContentsElementId, "stopCamera"),
      ExecuteJs(kWebContentsElementId, "stopMic"),
      WaitForHide(ContentSettingImageView::kMediaActivityIndicatorElementId));
}
