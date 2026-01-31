// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager_factory.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/color/color_provider.h"

namespace {

// Helper class to manage mojo remote to the WebUIToolbarUI.
class MockBrowserControlsServiceFactory {
 public:
  explicit MockBrowserControlsServiceFactory(WebUIToolbarUI* ui) {
    ui->BindInterface(factory_remote_.BindNewPipeAndPassReceiver());
  }

  // Not movable or copyable.
  MockBrowserControlsServiceFactory(const MockBrowserControlsServiceFactory&) =
      delete;
  MockBrowserControlsServiceFactory& operator=(
      const MockBrowserControlsServiceFactory&) = delete;

  void CreatePageHandler() {
    factory_remote_->CreateBrowserControls(
        mock_page_.BindAndGetRemote(),
        service_remote_.BindNewPipeAndPassReceiver());
    factory_remote_.FlushForTesting();
  }

  bool is_bound() { return factory_remote_.is_bound(); }
  bool is_connected() { return factory_remote_.is_connected(); }

  MockReloadButtonPage& mock_page() { return mock_page_; }

 private:
  testing::StrictMock<MockReloadButtonPage> mock_page_;
  mojo::Remote<browser_controls_api::mojom::BrowserControlsFactory>
      factory_remote_;
  mojo::Remote<browser_controls_api::mojom::BrowserControlsService>
      service_remote_;
};

}  // namespace

// Test fixture for WebUIToolbarUI.
class WebUIToolbarUITest : public ChromeViewsTestBase {
 public:
  WebUIToolbarUITest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUIInProcessResourceLoadingV2},
        {});
  }
  ~WebUIToolbarUITest() override = default;

  // Not movable or copyable.
  WebUIToolbarUITest(const WebUIToolbarUITest&) = delete;
  WebUIToolbarUITest& operator=(const WebUIToolbarUITest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();

    content::WebContents::CreateParams create_params(profile_.get());
    web_contents_ = content::WebContents::Create(create_params);

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());

    ui_ = std::make_unique<WebUIToolbarUI>(web_ui_.get());

    mock_command_updater_ =
        std::make_unique<testing::NiceMock<MockCommandUpdater>>();
    ui_->SetCommandUpdaterForTesting(mock_command_updater_.get());
  }

  void TearDown() override {
    ui_.reset();
    web_ui_.reset();
    web_contents_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  WebUIToolbarUI* ui() { return ui_.get(); }
  content::WebContents* web_contents() { return web_contents_.get(); }
  Profile* profile() { return profile_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<WebUIToolbarUI> ui_;
  std::unique_ptr<testing::NiceMock<MockCommandUpdater>> mock_command_updater_;
};

// Tests that OnNavigationStatusChanged and OnDevToolsStatusChanged call the
// browser controls service with the correct parameters.
TEST_F(WebUIToolbarUITest, SetReloadButtonState) {
  MockBrowserControlsServiceFactory factory(ui());
  factory.CreatePageHandler();

  EXPECT_CALL(factory.mock_page(),
              OnNavigationStatusChanged(
                  browser_controls_api::mojom::NavigationState::kLoading))
      .Times(1);
  ui()->OnNavigationStatusChanged(
      browser_controls_api::mojom::NavigationState::kLoading);
  factory.mock_page().FlushForTesting();

  EXPECT_CALL(factory.mock_page(),
              OnNavigationStatusChanged(
                  browser_controls_api::mojom::NavigationState::kNotLoading))
      .Times(1);
  ui()->OnNavigationStatusChanged(
      browser_controls_api::mojom::NavigationState::kNotLoading);
  factory.mock_page().FlushForTesting();

  EXPECT_CALL(factory.mock_page(),
              OnDevToolsStatusChanged(
                  browser_controls_api::mojom::DevToolsState::kConnected))
      .Times(1);
  ui()->OnDevToolsStatusChanged(
      browser_controls_api::mojom::DevToolsState::kConnected);
  factory.mock_page().FlushForTesting();

  EXPECT_CALL(factory.mock_page(),
              OnDevToolsStatusChanged(
                  browser_controls_api::mojom::DevToolsState::kDisconnected))
      .Times(1);
  ui()->OnDevToolsStatusChanged(
      browser_controls_api::mojom::DevToolsState::kDisconnected);
  factory.mock_page().FlushForTesting();
}

// Tests that the BindInterface method for PageHandlerFactory works correctly.
TEST_F(WebUIToolbarUITest, BindPageHandlerFactory) {
  MockBrowserControlsServiceFactory factory(ui());

  EXPECT_TRUE(factory.is_bound());
  EXPECT_TRUE(factory.is_connected());
}

// Tests that the CreatePageHandler method instantiates the page handler.
TEST_F(WebUIToolbarUITest, CreatePageHandler) {
  MockBrowserControlsServiceFactory factory(ui());

  factory.CreatePageHandler();
  EXPECT_THAT(ui()->browser_controls_service_for_testing(), testing::NotNull());
}

// Tests that CreatePageHandler handles a null CommandUpdater gracefully.
TEST_F(WebUIToolbarUITest, CreatePageHandler_NullCommandUpdater) {
  // Set command updater to null to simulate the crash scenario.
  ui()->SetCommandUpdaterForTesting(nullptr);

  MockBrowserControlsServiceFactory factory(ui());

  factory.CreatePageHandler();
  // Expect page handler is NOT created.
  EXPECT_THAT(ui()->browser_controls_service_for_testing(), testing::IsNull());
}

// Tests that PopulateLocalResourceLoaderConfig provides the theme source.
TEST_F(WebUIToolbarUITest, PopulateLocalResourceLoaderConfig) {
  // Create a mock ColorProvider.
  ui::ColorProvider color_provider;

  // Set the ColorProvider for testing via ThemeColorsSourceManager.
  auto* theme_colors_manager =
      ThemeColorsSourceManagerFactory::GetForProfile(profile());
  ASSERT_THAT(theme_colors_manager, testing::NotNull());
  theme_colors_manager->SetColorProviderForTesting(&color_provider);

  blink::mojom::LocalResourceLoaderConfig config;
  ui()->PopulateLocalResourceLoaderConfig(
      &config, url::Origin::Create(GURL("chrome://webui-toolbar.top-chrome/")));

  // Verify that the color CSS is added.
  url::Origin theme_origin = url::Origin::Create(GURL("chrome://theme/"));
  auto source_it = config.sources.find(theme_origin);
  ASSERT_TRUE(source_it != config.sources.end());

  auto resource_it = source_it->second->path_to_resource_map.find("colors.css");
  ASSERT_TRUE(resource_it != source_it->second->path_to_resource_map.end());
  EXPECT_TRUE(resource_it->second->is_response_body());
}

// Test fixture for WebUIToolbarUIConfig.
class WebUIToolbarUIConfigTest : public testing::Test {
 public:
  WebUIToolbarUIConfigTest() = default;
  ~WebUIToolbarUIConfigTest() override = default;

  // Not movable or copyable.
  WebUIToolbarUIConfigTest(const WebUIToolbarUIConfigTest&) = delete;
  WebUIToolbarUIConfigTest& operator=(const WebUIToolbarUIConfigTest&) = delete;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Tests that the IsWebUIEnabled method returns the correct value based on the
// feature flag.
TEST_F(WebUIToolbarUIConfigTest, IsWebUIEnabled) {
  TestingProfile profile;
  WebUIToolbarConfig config;

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton}, {});
    EXPECT_TRUE(config.IsWebUIEnabled(&profile));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kWebUIReloadButton);
    EXPECT_FALSE(config.IsWebUIEnabled(&profile));
  }
}
