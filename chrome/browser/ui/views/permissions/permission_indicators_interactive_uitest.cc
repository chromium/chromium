// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/features.h"
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
    scoped_feature_list_.InitWithFeatures(
        {}, {content_settings::features::kLeftHandSideActivityIndicators});
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
  // verifies its icon. With the WebUI toolbar, content setting icons are
  // rendered in WebUI rather than as `ContentSettingImageView`s, so the
  // underlying model is queried instead.
  //
  // TODO(crbug.com/562469854): Replace this with a content setting image test
  // accessor that implements the view methods for the WebUI version, following
  // `page_actions::PageActionTestAccessor`. That would centralize the dispatch
  // below and let tests catch rendering gaps that are invisible at the model
  // layer, e.g. the WebUI icons do not support badges at all.
  auto VerifyMediaStreamIndicator(const gfx::VectorIcon* expected_icon) {
    return AfterShow(
        ContentSettingImageModel::kMediaStreamIconElementId,
        base::BindLambdaForTesting(
            [this, expected_icon](ui::TrackedElement* element) {
              const gfx::VectorIcon* icon = nullptr;
              const gfx::VectorIcon* icon_badge = nullptr;
              if (features::IsWebUILocationBarEnabled()) {
                // `element` is a `TrackedElementWebUI` wrapping a DOM node, so
                // there is no view to inspect. Check the model that drives it.
                auto* location_bar = static_cast<WebUILocationBar*>(
                    BrowserWindow::FromBrowser(browser())->GetLocationBar());
                auto* model =
                    location_bar->content_setting_image_control().GetModel(
                        ContentSettingImageModel::ImageType::kMediaStream);
                ASSERT_TRUE(model);
                icon = model->icon();
                icon_badge = model->get_icon_badge();
              } else {
                // `element` is the `ContentSettingImageView` itself.
                auto* element_view = AsView<ContentSettingImageView>(element);
                ASSERT_TRUE(element_view);
                icon = element_view->get_icon_for_testing();
                icon_badge = element_view->get_icon_badge_for_testing();
              }
              EXPECT_EQ(icon, expected_icon);
              // Permission is granted, there is no badge.
              EXPECT_EQ(icon_badge, &gfx::VectorIcon::EmptyIcon());
            }));
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
      VerifyMediaStreamIndicator(
          &(features::IsRoundedIconsEnabled()
                ? vector_icons::kVideocamIcon
                : vector_icons::kVideocamChromeRefreshOldIcon)),
      ExecuteJs(kWebContentsElementId, "stopCamera"),
      WaitForHide(ContentSettingImageModel::kMediaStreamIconElementId));
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
      WaitForHide(ContentSettingImageModel::kMediaStreamIconElementId));
}
