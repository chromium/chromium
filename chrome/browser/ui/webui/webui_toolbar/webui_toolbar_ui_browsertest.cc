// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/theme_colors_source_manager.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager_factory.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/color/color_provider.h"

namespace {

// Helper class to manage mojo remote to the WebUIToolbarUI.
class MockBrowserControlsServiceConnection {
 public:
  explicit MockBrowserControlsServiceConnection(WebUIToolbarUI* ui) {
    ui->BindInterface(service_remote_.BindNewPipeAndPassReceiver());
  }

  // Not movable or copyable.
  MockBrowserControlsServiceConnection(
      const MockBrowserControlsServiceConnection&) = delete;
  MockBrowserControlsServiceConnection& operator=(
      const MockBrowserControlsServiceConnection&) = delete;

  void RegisterObserver() {
    service_remote_->Bind(base::BindOnce(
        [](MockBrowserControlsServiceConnection* self,
           base::expected<browser_controls_api::mojom::InitialStatePtr,
                          mojo_base::mojom::ErrorPtr> result) {
          ASSERT_TRUE(result.has_value());
          self->mock_observer_.Bind(std::move(result.value()->update_stream));
        },
        base::Unretained(this)));
    service_remote_.FlushForTesting();
  }

  void FlushForTesting() { service_remote_.FlushForTesting(); }

  bool is_bound() { return service_remote_.is_bound(); }
  bool is_connected() { return service_remote_.is_connected(); }

  MockReloadButtonPage& mock_observer() { return mock_observer_; }

 private:
  testing::StrictMock<MockReloadButtonPage> mock_observer_;
  mojo::Remote<browser_controls_api::mojom::BrowserControlsService>
      service_remote_;
};

class BrowserControlsDelegate
    : public browser_controls_api::BrowserControlsService::Delegate {
 public:
  BrowserControlsDelegate() = default;
  ~BrowserControlsDelegate() override = default;

  void HandleContextMenu(browser_controls_api::mojom::ContextMenuType menu_type,
                         gfx::Point viewport_coordinate_css_pixels,
                         ui::mojom::MenuSourceType source) override {}
  void OnPageInitialized() override {}
  void PermitLaunchUrl() override {}
};

// Test fixture for WebUIToolbarUI. These tests test the connectivity between
// the components in the web ui toolbar component. It is not intended to deeply
// exercise each component. For example, it might test that data flows
// correctly from the browser controls service to some observer, but it will
// not validate the behaviours.
class WebUIToolbarUIBrowserTest : public InProcessBrowserTest,
                                  public WebUIToolbarUI::DependencyProvider {
 public:
  WebUIToolbarUIBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUIInProcessResourceLoadingV2},
        {});
  }
  ~WebUIToolbarUIBrowserTest() override = default;

  // Not movable or copyable.
  WebUIToolbarUIBrowserTest(const WebUIToolbarUIBrowserTest&) = delete;
  WebUIToolbarUIBrowserTest& operator=(const WebUIToolbarUIBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(chrome_test_utils::GetActiveWebContents(this));
    ui_ = std::make_unique<WebUIToolbarUI>(web_ui_.get());
    ui_->Init(this);
  }

  void TearDownOnMainThread() override {
    ui_.reset();
    web_ui_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // WebUIToolbarUI::DependencyProvider:
  browser_controls_api::BrowserControlsService::Delegate* GetDelegate()
      override {
    return &delegate_;
  }
  std::unique_ptr<browser_controls_api::NavigationControlsStateFetcher>
  GetNavigationControlsStateFetcher() override {
    return std::make_unique<
        browser_controls_api::NavigationControlsStateFetcherImpl>(
        base::BindRepeating(
            [&] { return CreateValidNavigationControlsState(); }));
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  WebUIToolbarUI* ui() { return ui_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  BrowserControlsDelegate delegate_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<WebUIToolbarUI> ui_;
};

// Tests that OnNavigationControlsStateChanged calls the browser controls
// observer with the correct parameters.
IN_PROC_BROWSER_TEST_F(WebUIToolbarUIBrowserTest, SetReloadButtonState) {
  MockBrowserControlsServiceConnection connection(ui());

  auto state = CreateValidNavigationControlsState();
  state->reload_control_state->is_navigation_loading = true;
  connection.RegisterObserver();

  EXPECT_CALL(connection.mock_observer(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_navigation_loading,
                      true))))))
      .Times(1);
  ui()->OnNavigationControlsStateChanged(std::move(state));
  connection.mock_observer().FlushForTesting();
}

// Tests that the BindInterface method for BrowserControlsService works
// correctly.
IN_PROC_BROWSER_TEST_F(WebUIToolbarUIBrowserTest, BindService) {
  MockBrowserControlsServiceConnection connection(ui());

  EXPECT_TRUE(connection.is_bound());
  EXPECT_TRUE(connection.is_connected());
}

// Tests that connecting to the service instantiates the BrowserControlsService.
IN_PROC_BROWSER_TEST_F(WebUIToolbarUIBrowserTest, CreateService) {
  MockBrowserControlsServiceConnection connection(ui());

  EXPECT_THAT(ui()->browser_controls_service_for_testing(), testing::NotNull());
}

// Tests that Service creation handles a null CommandUpdater gracefully.
IN_PROC_BROWSER_TEST_F(WebUIToolbarUIBrowserTest,
                       CreateService_NullCommandUpdater) {
  TestingProfile test_profile;
  // Simulate a null browser window interface for a web content to simulate
  // browser shutdown conditions. For this edge case, we expect the service to
  // gracefully abort the bind attempt.
  content::WebContents::CreateParams create_params(&test_profile);
  auto dummy_content = content::WebContents::Create(create_params);
  webui::SetBrowserWindowInterface(dummy_content.get(), nullptr);

  web_ui()->set_web_contents(dummy_content.get());

  MockBrowserControlsServiceConnection connection(ui());
  // This line is necessary, because there is a defect in mojo's is_connected()
  // which erroneously return true without this, even if the remote is not
  // actually connected.
  connection.FlushForTesting();

  EXPECT_FALSE(connection.is_connected());
}

// Tests that PopulateLocalResourceLoaderConfig provides the theme source.
IN_PROC_BROWSER_TEST_F(WebUIToolbarUIBrowserTest,
                       PopulateLocalResourceLoaderConfig) {
  // Create a mock ColorProvider.
  ui::ColorProvider color_provider;

  // Set the ColorProvider for testing via ThemeColorsSourceManager.
  auto* profile = chrome_test_utils::GetProfile(this);
  auto* theme_colors_manager =
      ThemeColorsSourceManagerFactory::GetForProfile(profile);
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

}  // namespace
