// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
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

  bool is_bound() { return service_remote_.is_bound(); }
  bool is_connected() { return service_remote_.is_connected(); }

  MockReloadButtonPage& mock_observer() { return mock_observer_; }

 private:
  testing::StrictMock<MockReloadButtonPage> mock_observer_;
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
    ui_->SetDelegate(&delegate_);

    ON_CALL(delegate_, GetNavigationControlsState()).WillByDefault([]() {
      return CreateValidNavigationControlsState();
    });

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
  testing::NiceMock<MockWebWebUIToolbarDelegate> delegate_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<WebUIToolbarUI> ui_;
  std::unique_ptr<testing::NiceMock<MockCommandUpdater>> mock_command_updater_;
};

// Tests that OnNavigationControlsStateChanged calls the browser controls
// observer with the correct parameters.
TEST_F(WebUIToolbarUITest, SetReloadButtonState) {
  MockBrowserControlsServiceConnection connection(ui());
  connection.RegisterObserver();

  auto state = CreateValidNavigationControlsState();
  state->reload_control_state->is_navigation_loading = true;

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

  state = CreateValidNavigationControlsState();
  state->reload_control_state->is_navigation_loading = false;

  EXPECT_CALL(connection.mock_observer(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_navigation_loading,
                      false))))))
      .Times(1);
  ui()->OnNavigationControlsStateChanged(std::move(state));
  connection.mock_observer().FlushForTesting();

  state = CreateValidNavigationControlsState();
  state->reload_control_state->is_devtools_connected = true;

  EXPECT_CALL(connection.mock_observer(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_devtools_connected,
                      true))))))
      .Times(1);
  ui()->OnNavigationControlsStateChanged(std::move(state));
  connection.mock_observer().FlushForTesting();

  state = CreateValidNavigationControlsState();
  state->reload_control_state->is_devtools_connected = false;

  EXPECT_CALL(connection.mock_observer(),
              OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
                  &browser_controls_api::mojom::NavigationControlsState::
                      reload_control_state,
                  testing::Pointee(testing::Field(
                      &browser_controls_api::mojom::ReloadControlState::
                          is_devtools_connected,
                      false))))))
      .Times(1);
  ui()->OnNavigationControlsStateChanged(std::move(state));
  connection.mock_observer().FlushForTesting();
}

// Tests that OnNavigationControlsStateChanged calls the browser controls
// observer with the correct parameters.
TEST_F(WebUIToolbarUITest, OnTabSplitStatusChanged) {
  MockBrowserControlsServiceConnection connection(ui());
  connection.RegisterObserver();

  auto state = CreateValidNavigationControlsState();
  state->split_tabs_control_state->is_current_tab_split = true;
  state->split_tabs_control_state->location =
      browser_controls_api::mojom::SplitTabActiveLocation::kStart;

  EXPECT_CALL(
      connection.mock_observer(),
      OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
          &browser_controls_api::mojom::NavigationControlsState::
              split_tabs_control_state,
          testing::Pointee(testing::AllOf(
              testing::Field(&browser_controls_api::mojom::
                                 SplitTabsControlState::is_current_tab_split,
                             true),
              testing::Field(
                  &browser_controls_api::mojom::SplitTabsControlState::location,
                  browser_controls_api::mojom::SplitTabActiveLocation::
                      kStart)))))))
      .Times(1);
  ui()->OnNavigationControlsStateChanged(std::move(state));
  connection.mock_observer().FlushForTesting();
}

// Tests that OnNavigationControlsStateChanged calls the browser controls
// observer with the correct parameters.
TEST_F(WebUIToolbarUITest, OnButtonPinStateChanged) {
  MockBrowserControlsServiceConnection connection(ui());
  connection.RegisterObserver();

  auto state = CreateValidNavigationControlsState();
  state->split_tabs_control_state->is_pinned = true;

  EXPECT_CALL(
      connection.mock_observer(),
      OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
          &browser_controls_api::mojom::NavigationControlsState::
              split_tabs_control_state,
          testing::Pointee(testing::Field(
              &browser_controls_api::mojom::SplitTabsControlState::is_pinned,
              true))))))
      .Times(1);
  ui()->OnNavigationControlsStateChanged(std::move(state));
  connection.mock_observer().FlushForTesting();

  state = CreateValidNavigationControlsState();
  state->split_tabs_control_state->is_pinned = false;

  EXPECT_CALL(
      connection.mock_observer(),
      OnNavigationControlsStateChanged(testing::Pointee(testing::Field(
          &browser_controls_api::mojom::NavigationControlsState::
              split_tabs_control_state,
          testing::Pointee(testing::Field(
              &browser_controls_api::mojom::SplitTabsControlState::is_pinned,
              false))))))
      .Times(1);
  ui()->OnNavigationControlsStateChanged(std::move(state));
  connection.mock_observer().FlushForTesting();
}

// Tests that the BindInterface method for BrowserControlsService works
// correctly.
TEST_F(WebUIToolbarUITest, BindService) {
  MockBrowserControlsServiceConnection connection(ui());

  EXPECT_TRUE(connection.is_bound());
  EXPECT_TRUE(connection.is_connected());
}

// Tests that connecting to the service instantiates the BrowserControlsService.
TEST_F(WebUIToolbarUITest, CreateService) {
  MockBrowserControlsServiceConnection connection(ui());

  EXPECT_THAT(ui()->browser_controls_service_for_testing(), testing::NotNull());
}

// Tests that Service creation handles a null CommandUpdater gracefully.
TEST_F(WebUIToolbarUITest, CreateService_NullCommandUpdater) {
  // Set command updater to null to simulate the crash scenario.
  ui()->SetCommandUpdaterForTesting(nullptr);

  MockBrowserControlsServiceConnection connection(ui());

  // Expect service is NOT created.
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
        {features::kInitialWebUI, features::kWebUIReloadButton},
        {features::kWebUISplitTabsButton});
    EXPECT_TRUE(config.IsWebUIEnabled(&profile));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUISplitTabsButton},
        {features::kWebUIReloadButton});
    EXPECT_TRUE(config.IsWebUIEnabled(&profile));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI},
        {features::kWebUIReloadButton, features::kWebUISplitTabsButton});
    EXPECT_FALSE(config.IsWebUIEnabled(&profile));
  }
}
