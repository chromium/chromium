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
class ChipAnimationObserver : PermissionChipView::Observer {
 public:
  enum class QuitOnEvent {
    kExpand,
    kCollapse,
    kVisibiltyTrue,
    kVisibiltyFalse,
  };

  explicit ChipAnimationObserver(PermissionChipView* chip) {
    observation_.Observe(chip);
  }

  void WaitForChip() { loop_.Run(); }

  void OnExpandAnimationEnded() override {
    if (quiet_on_event == QuitOnEvent::kExpand) {
      loop_.Quit();
    }
  }
  void OnCollapseAnimationEnded() override {
    if (quiet_on_event == QuitOnEvent::kCollapse) {
      loop_.Quit();
    }
  }

  void OnChipVisibilityChanged(bool is_visible) override {
    if (quiet_on_event == QuitOnEvent::kVisibiltyTrue && is_visible) {
      loop_.Quit();
      return;
    }

    if (quiet_on_event == QuitOnEvent::kVisibiltyFalse && !is_visible) {
      loop_.Quit();
    }
  }

  base::ScopedObservation<PermissionChipView, PermissionChipView::Observer>
      observation_{this};
  base::RunLoop loop_;
  QuitOnEvent quiet_on_event = QuitOnEvent::kExpand;
};
}  // namespace

class LHSIndicatorsUiBrowserTest : public UiBrowserTest {
 public:
  enum class TargetViewToVerify { kLocationBar, kPageInfo };

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

    InitMainFrame();

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
    views::View* view_to_verify = nullptr;
    if (target_ == TargetViewToVerify::kLocationBar) {
      view_to_verify = GetLocationBarView(browser());
    } else if (target_ == TargetViewToVerify::kPageInfo) {
      view_to_verify = GetDashboardController()->page_info_for_testing();
    }

    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
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
    content::RenderFrameHost* main_rfh =
        ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                                  GetURL(), 1);
    web_contents()->Focus();
    return main_rfh;
  }

  void UpdatePageInfo() {
    target_ = TargetViewToVerify::kPageInfo;
    PermissionDashboardController* controller = GetDashboardController();
    controller->ShowPageInfoDialogForTesting();

    // Override origin in PageInfo to avoid flakiness due to different port.
    auto* bubble_view =
        static_cast<PageInfoBubbleView*>(controller->page_info_for_testing());
    std::u16string site_name = u"test.com";
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(bubble_view->presenter_for_testing()->GetSubjectNameForDisplay(),
              site_name);
  }

  void ExpandIndicator(std::string js) {
    ChipAnimationObserver chip_animation_observer(GetIndicatorChip());
    chip_animation_observer.quiet_on_event =
        ChipAnimationObserver::QuitOnEvent::kExpand;

    EXPECT_TRUE(content::ExecJs(web_contents(), js));

    // Wait until chip expands.
    chip_animation_observer.WaitForChip();

    EXPECT_TRUE(GetIndicatorChip()->GetVisible());
    EXPECT_TRUE(GetDashboardController()->is_verbose());
  }

  void CollapseIndicator() {
    ChipAnimationObserver chip_animation_observer(GetIndicatorChip());
    chip_animation_observer.quiet_on_event =
        ChipAnimationObserver::QuitOnEvent::kCollapse;
    // Wait until chip collapses.
    chip_animation_observer.WaitForChip();

    EXPECT_TRUE(GetIndicatorChip()->GetVisible());
    EXPECT_FALSE(GetDashboardController()->is_verbose());
  }

  void HideIndicator(std::string js) {
    ChipAnimationObserver chip_animation_observer(GetIndicatorChip());
    chip_animation_observer.quiet_on_event =
        ChipAnimationObserver::QuitOnEvent::kVisibiltyFalse;

    EXPECT_TRUE(content::ExecJs(web_contents(), js));

    // Wait until chip hides.
    chip_animation_observer.WaitForChip();

    EXPECT_FALSE(GetIndicatorChip()->GetVisible());
    EXPECT_FALSE(GetDashboardController()->is_verbose());
  }

  PermissionChipView* GetIndicatorChip() {
    return GetLocationBarView(browser())
        ->permission_dashboard_controller()
        ->permission_dashboard_view()
        ->GetIndicatorChip();
  }

  PermissionDashboardController* GetDashboardController() {
    return GetLocationBarView(browser())->permission_dashboard_controller();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  TargetViewToVerify target_ = TargetViewToVerify::kLocationBar;

 private:
  // Disable the permission chip animation. This happens automatically in pixel
  // test mode, but without doing this explicitly, the test will fail when run
  // interactively.
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_camera) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);

  GetDashboardController()->DoNotCollapseForTesting();

  ExpandIndicator("requestCamera()");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_microphone) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_ALLOW);
  GetDashboardController()->DoNotCollapseForTesting();

  ExpandIndicator("requestMicrophone()");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_cameraandmicrophone) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_ALLOW);

  GetDashboardController()->DoNotCollapseForTesting();

  ExpandIndicator("requestCameraAndMicrophone()");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_camera_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_BLOCK);

  GetDashboardController()->DoNotCollapseForTesting();

  ExpandIndicator("requestCamera()");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_microphone_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_BLOCK);

  GetDashboardController()->DoNotCollapseForTesting();

  ExpandIndicator("requestMicrophone()");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_cameraandmicrophone_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_BLOCK);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_BLOCK);

  GetDashboardController()->DoNotCollapseForTesting();

  ExpandIndicator("requestCameraAndMicrophone()");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_PageInfo_camera) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);

  ExpandIndicator("requestCamera()");

  UpdatePageInfo();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_PageInfo_mic) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_ALLOW);

  ExpandIndicator("requestMicrophone()");

  UpdatePageInfo();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_PageInfo_camera_and_mic) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_ALLOW);

  ExpandIndicator("requestCameraAndMicrophone()");

  UpdatePageInfo();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_PageInfo_camera_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_BLOCK);

  ExpandIndicator("requestCamera()");

  UpdatePageInfo();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_PageInfo_mic_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_BLOCK);

  ExpandIndicator("requestMicrophone()");

  UpdatePageInfo();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest,
                       InvokeUi_PageInfo_camera_and_mic_blocked) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_BLOCK);
  SetPermission(ContentSettingsType::MEDIASTREAM_MIC,
                ContentSetting::CONTENT_SETTING_BLOCK);

  ExpandIndicator("requestCameraAndMicrophone()");

  UpdatePageInfo();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LHSIndicatorsUiBrowserTest, InvokeUi_Camera_twice) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                ContentSetting::CONTENT_SETTING_ALLOW);
  InitMainFrame();

  ExpandIndicator("requestCamera()");

  CollapseIndicator();

  HideIndicator("stopCamera()");

  // Request Camera for the second time.
  ChipAnimationObserver chip_animation_observer(GetIndicatorChip());
  chip_animation_observer.quiet_on_event =
      ChipAnimationObserver::QuitOnEvent::kVisibiltyTrue;

  EXPECT_TRUE(content::ExecJs(web_contents(), "requestCamera()"));

  // Wait until chip expands.
  chip_animation_observer.WaitForChip();

  EXPECT_TRUE(GetIndicatorChip()->GetVisible());
  // Second camera request does not trigger verbose indicator.
  EXPECT_FALSE(GetDashboardController()->is_verbose());

  target_ = TargetViewToVerify::kLocationBar;

  ShowAndVerifyUi();
}
