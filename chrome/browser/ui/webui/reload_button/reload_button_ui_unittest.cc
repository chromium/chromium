// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
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

// Helper class to manage mojo remote to the ReloadButtonUI.
class MockReloadButtonPageHandlerFactory {
 public:
  explicit MockReloadButtonPageHandlerFactory(ReloadButtonUI* ui) {
    ui->BindInterface(factory_remote_.BindNewPipeAndPassReceiver());
  }

  // Not movable or copyable.
  MockReloadButtonPageHandlerFactory(
      const MockReloadButtonPageHandlerFactory&) = delete;
  MockReloadButtonPageHandlerFactory& operator=(
      const MockReloadButtonPageHandlerFactory&) = delete;

  void CreatePageHandler() {
    factory_remote_->CreatePageHandler(
        mock_page_.BindAndGetRemote(),
        handler_remote_.BindNewPipeAndPassReceiver());
    factory_remote_.FlushForTesting();
  }

  bool is_bound() { return factory_remote_.is_bound(); }
  bool is_connected() { return factory_remote_.is_connected(); }

  MockReloadButtonPage& mock_page() { return mock_page_; }

 private:
  testing::StrictMock<MockReloadButtonPage> mock_page_;
  mojo::Remote<reload_button::mojom::PageHandlerFactory> factory_remote_;
  mojo::Remote<reload_button::mojom::PageHandler> handler_remote_;
};

}  // namespace

// Test fixture for ReloadButtonUI.
class ReloadButtonUITest : public ChromeViewsTestBase {
 public:
  ReloadButtonUITest() = default;
  ~ReloadButtonUITest() override = default;

  // Not movable or copyable.
  ReloadButtonUITest(const ReloadButtonUITest&) = delete;
  ReloadButtonUITest& operator=(const ReloadButtonUITest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton}, {});

    profile_ = std::make_unique<TestingProfile>();

    content::WebContents::CreateParams create_params(profile_.get());
    web_contents_ = content::WebContents::Create(create_params);

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());

    ui_ = std::make_unique<ReloadButtonUI>(web_ui_.get());

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

  ReloadButtonUI* ui() { return ui_.get(); }
  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<ReloadButtonUI> ui_;
  std::unique_ptr<testing::NiceMock<MockCommandUpdater>> mock_command_updater_;
};

// Tests that SetReloadButtonState calls the page handler with the correct
// parameters.
TEST_F(ReloadButtonUITest, SetReloadButtonState) {
  MockReloadButtonPageHandlerFactory factory(ui());
  factory.CreatePageHandler();

  EXPECT_CALL(factory.mock_page(), SetReloadButtonState(true, false)).Times(1);
  ui()->SetReloadButtonState(true, false);
  factory.mock_page().FlushForTesting();

  EXPECT_CALL(factory.mock_page(), SetReloadButtonState(false, true)).Times(1);
  ui()->SetReloadButtonState(false, true);
  factory.mock_page().FlushForTesting();
}

// Tests that the BindInterface method for PageHandlerFactory works correctly.
TEST_F(ReloadButtonUITest, BindPageHandlerFactory) {
  MockReloadButtonPageHandlerFactory factory(ui());

  EXPECT_TRUE(factory.is_bound());
  EXPECT_TRUE(factory.is_connected());
}

// Tests that the CreatePageHandler method instantiates the page handler.
TEST_F(ReloadButtonUITest, CreatePageHandler) {
  MockReloadButtonPageHandlerFactory factory(ui());

  factory.CreatePageHandler();
  EXPECT_THAT(ui()->page_handler_for_testing(), testing::NotNull());
}

// Tests that PopulateLocalResourceLoaderConfig provides the theme source.
TEST_F(ReloadButtonUITest, PopulateLocalResourceLoaderConfig) {
  ui::ColorProvider color_provider;
  ui()->SetColorProviderForTesting(&color_provider);

  auto config = blink::mojom::LocalResourceLoaderConfig::New();
  ui()->PopulateLocalResourceLoaderConfig(
      config.get(), url::Origin::Create(GURL("chrome://reload-button/")));

  auto origin = url::Origin::Create(GURL("chrome://theme"));

  ASSERT_TRUE(config->sources.contains(origin));
  EXPECT_TRUE(
      config->sources[origin]->path_to_resource_map.contains("colors.css"));
}

// Test fixture for ReloadButtonUIConfig.
class ReloadButtonUIConfigTest : public testing::Test {
 public:
  ReloadButtonUIConfigTest() = default;
  ~ReloadButtonUIConfigTest() override = default;

  // Not movable or copyable.
  ReloadButtonUIConfigTest(const ReloadButtonUIConfigTest&) = delete;
  ReloadButtonUIConfigTest& operator=(const ReloadButtonUIConfigTest&) = delete;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Tests that the IsWebUIEnabled method returns the correct value based on the
// feature flag.
TEST_F(ReloadButtonUIConfigTest, IsWebUIEnabled) {
  TestingProfile profile;
  ReloadButtonUIConfig config;

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
