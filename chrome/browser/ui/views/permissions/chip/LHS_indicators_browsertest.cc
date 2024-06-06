// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "url/gurl.h"

namespace {
constexpr char kRequestCamera[] = R"(
    new Promise(async resolve => {
      var constraints = { video: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";

constexpr char kRequestMic[] = R"(
    new Promise(async resolve => {
      var constraints = { audio: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";

constexpr char kRequestCameraAndMic[] = R"(
    new Promise(async resolve => {
      var constraints = { audio: true, video: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";
}  // namespace

class LHSIndicatorsUiBrowserTest : public UiBrowserTest {
 public:
  LHSIndicatorsUiBrowserTest() {
    scoped_features_.InitAndEnableFeature(
        content_settings::features::kLeftHandSideActivityIndicators);
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~LHSIndicatorsUiBrowserTest() override {}

  void SetUpOnMainThread() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();

    // Override url in the omnibox to avoid test flakiness due to different port
    // in the original url.
    std::u16string url_override(u"https://www.test.com/");
    OverrideVisibleUrlInLocationBar(url_override);

    UiBrowserTest::SetUpOnMainThread();
  }

  void OverrideVisibleUrlInLocationBar(const std::u16string& text) {
    OmniboxView* omnibox_view = GetLocationBarView(browser())->GetOmniboxView();
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

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {}

  bool VerifyUi() override {
    LocationBarView* const location_bar = GetLocationBarView(browser());
    PermissionDashboardController* permission_dashboard_controller =
        location_bar->permission_dashboard_controller();

    if (!permission_dashboard_controller) {
      return false;
    }
    PermissionDashboardView* permission_dashboard_view =
        permission_dashboard_controller->permission_dashboard_view();

    if (!permission_dashboard_view ||
        !permission_dashboard_view->GetVisible()) {
      return false;
    }
    PermissionChipView* lhs_indicators_chip =
        permission_dashboard_view->GetIndicatorChip();

    if (!lhs_indicators_chip || !lhs_indicators_chip->GetVisible()) {
      return false;
    }

    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    views::View* view_to_verify = view_to_verify_;
    view_to_verify_ = nullptr;
    return VerifyPixelUi(view_to_verify, test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

  LocationBarView* GetLocationBarView(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->location_bar();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  void SetPermission(ContentSettingsType type, ContentSetting setting) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());

    map->SetContentSettingDefaultScope(GetURL(), GetURL(), type, setting);
  }

  content::RenderFrameHost* InitMainFrame() {
    content::WebContents* embedder_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* main_rfh =
        ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                                  GetURL(), 1);
    embedder_contents->Focus();
    return main_rfh;
  }

  void SetIndicatorsViewToCheck() {
    LocationBarView* const location_bar = GetLocationBarView(browser());
    PermissionDashboardController* permission_dashboard_controller =
        location_bar->permission_dashboard_controller();

    ASSERT_TRUE(permission_dashboard_controller);

    // Prevernt the LHS indicator to collapse from the verbose state.
    permission_dashboard_controller->DoNotCollapseForTesting();

    view_to_verify_ =
        permission_dashboard_controller->permission_dashboard_view();
  }

  void SetPageInfoViewToCheck() {
    LocationBarView* const location_bar = GetLocationBarView(browser());
    PermissionDashboardController* permission_dashboard_controller =
        location_bar->permission_dashboard_controller();

    ASSERT_TRUE(permission_dashboard_controller);

    permission_dashboard_controller->ShowPageInfoDialogForTesting();
    view_to_verify_ = permission_dashboard_controller->page_info_for_testing();

    // Override origin in PageInfo to avoid flakiness due to different port.
    auto* bubble_view = static_cast<PageInfoBubbleView*>(view_to_verify_);
    std::u16string site_name = u"test.com";
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(bubble_view->presenter_for_testing()->GetSubjectNameForDisplay(),
              site_name);
  }

 private:
  // Disable the permission chip animation. This happens automatically in pixel
  // test mode, but without doing this explicitly, the test will fail when run
  // interactively.
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  raw_ptr<views::View> view_to_verify_;
};

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_camera) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  SetIndicatorsViewToCheck();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestCamera));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_microphone) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_ALLOW);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  SetIndicatorsViewToCheck();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestMic));

  ShowAndVerifyUi();
}

// TODO(crbug.com/344706072): re-enable this flaky test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_cameraandmicrophone DISABLED_InvokeUi_cameraandmicrophone
#else
#define MAYBE_InvokeUi_cameraandmicrophone InvokeUi_cameraandmicrophone
#endif
IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       MAYBE_InvokeUi_cameraandmicrophone) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_ALLOW);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  SetIndicatorsViewToCheck();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestCameraAndMic));

  ShowAndVerifyUi();
}

// TODO(crbug.com/344706072): re-enable this flaky test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_camera_blocked DISABLED_InvokeUi_camera_blocked
#else
#define MAYBE_InvokeUi_camera_blocked InvokeUi_camera_blocked
#endif
IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       MAYBE_InvokeUi_camera_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_BLOCK);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  SetIndicatorsViewToCheck();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestCamera));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_microphone_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_BLOCK);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  SetIndicatorsViewToCheck();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestMic));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_cameraandmicrophone_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_BLOCK);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_BLOCK);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  SetIndicatorsViewToCheck();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestCameraAndMic));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_PageInfo_camera) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestCamera));
  SetPageInfoViewToCheck();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_PageInfo_mic) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_ALLOW);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestMic));
  SetPageInfoViewToCheck();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_PageInfo_camera_and_mic) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_ALLOW);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestCameraAndMic));
  SetPageInfoViewToCheck();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_PageInfo_camera_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_BLOCK);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestCamera));
  SetPageInfoViewToCheck();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_PageInfo_mic_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_BLOCK);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestMic));
  SetPageInfoViewToCheck();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_PageInfo_camera_and_mic_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_BLOCK);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_BLOCK);

  content::RenderFrameHost* main_rfh = InitMainFrame();
  EXPECT_TRUE(content::ExecJs(main_rfh, kRequestCameraAndMic));
  SetPageInfoViewToCheck();

  ShowAndVerifyUi();
}
