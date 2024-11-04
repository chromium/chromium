// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_list.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

// Test implementation of PermissionUiSelector that always returns a canned
// decision.
class TestQuietNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      const Decision& canned_decision)
      : canned_decision_(canned_decision) {}
  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(canned_decision_);
  }

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override {
    return request_type == permissions::RequestType::kNotifications;
  }

 private:
  Decision canned_decision_;
};

class LocationBarViewQuietNotificationInteractiveUITest
    : public InteractiveFeaturePromoTest,
      public testing::WithParamInterface<bool> {
 public:
  // IPH feature should be explicitly enabled in test.
  LocationBarViewQuietNotificationInteractiveUITest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHPwaQuietNotificationFeature})) {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveFeaturePromoTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

  void GetContentSettingViews() {
    content_setting_views_ =
        &helper()->web_app_frame_toolbar()->GetContentSettingViewsForTesting();
  }

  ContentSettingImageView* GetNotificationView() {
    return *base::ranges::find(
        *content_setting_views_,
        ContentSettingImageModel::ImageType::NOTIFICATIONS,
        &ContentSettingImageView::GetType);
  }

  void InstallAndLaunchWebApp() {
    helper()->InstallAndLaunchWebApp(browser(), GetURL());

    // `PermissionRequestManagerTestApi` should be set after WebApp is
    // installed.
    test_api_ = std::make_unique<test::PermissionRequestManagerTestApi>(
        helper()->app_browser());
  }

  ui::ElementContext GetAppWindowElementContext() {
    return helper()->app_browser()->window()->GetElementContext();
  }

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  void SetCannedUiDecision() {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(
                permissions::PermissionUiSelector::QuietUiReason::
                    kEnabledInPrefs,
                std::nullopt)));
  }

 private:
  WebAppFrameToolbarTestHelper* helper() {
    return &web_app_frame_toolbar_helper_;
  }

  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
  raw_ptr<
      const std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>,
      DanglingUntriaged>
      content_setting_views_ = nullptr;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(LocationBarViewQuietNotificationInteractiveUITest,
                       QuietNotificationIPH) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kContentsWebViewsElementId);
  InstallAndLaunchWebApp();
  SetCannedUiDecision();
  RunTestSequenceInContext(
      GetAppWindowElementContext(), InstrumentTab(kContentsWebViewsElementId),
      ExecuteJs(kContentsWebViewsElementId, "requestNotification"),
      WaitForPromo(feature_engagement::kIPHPwaQuietNotificationFeature));
}

}  // namespace
