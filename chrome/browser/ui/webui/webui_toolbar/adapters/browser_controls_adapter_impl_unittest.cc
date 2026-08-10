
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter_impl.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_drag_state.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace browser_controls_api {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class BrowserControlsAdapterImplTest : public ChromeRenderViewHostTestHarness {
 public:
  BrowserControlsAdapterImplTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));
    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(Return(profile()));
    command_updater_ = std::make_unique<NiceMock<MockCommandUpdater>>();
    adapter_ = std::make_unique<BrowserControlsAdapterImpl>(
        &browser_window_interface_, command_updater_.get(), web_contents());
  }

  void TearDown() override {
    adapter_.reset();
    command_updater_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<MockCommandUpdater> command_updater_;
  std::unique_ptr<BrowserControlsAdapterImpl> adapter_;
};

TEST_F(BrowserControlsAdapterImplTest, NavigateText_HTTPAllowed) {
  GURL expected_url("https://www.example.com/");
  EXPECT_CALL(browser_window_interface_,
              OpenGURL(expected_url, WindowOpenDisposition::CURRENT_TAB));
  adapter_->NavigateText("https://www.example.com/");
}

TEST_F(BrowserControlsAdapterImplTest,
       NavigateText_PrivilegedSchemeBlockedOnRendererDrag) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(true);
  EXPECT_CALL(browser_window_interface_, OpenGURL(_, _)).Times(0);
  adapter_->NavigateText("chrome://settings");
}

TEST_F(BrowserControlsAdapterImplTest,
       NavigateText_PrivilegedSchemeWhenDragNotRendererTainted) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(false);
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, all drags are conservatively treated as renderer-originated to
  // enforce strict scheme validation, ensuring privileged schemes like
  // chrome:// are blocked even if the drag state is not flagged as
  // renderer-tainted.
  EXPECT_CALL(browser_window_interface_, OpenGURL(_, _)).Times(0);
#else
  EXPECT_CALL(
      browser_window_interface_,
      OpenGURL(GURL("chrome://settings"), WindowOpenDisposition::CURRENT_TAB));
#endif
  adapter_->NavigateText("chrome://settings");
}

TEST_F(BrowserControlsAdapterImplTest,
       Navigate_PrivilegedSchemeWhenDragNotRendererTainted) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(false);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(browser_window_interface_,
              OpenGURL(GURL("about:blank#blocked"),
                       WindowOpenDisposition::CURRENT_TAB));
#else
  EXPECT_CALL(
      browser_window_interface_,
      OpenGURL(GURL("chrome://settings"), WindowOpenDisposition::CURRENT_TAB));
#endif
  adapter_->Navigate(GURL("chrome://settings"));
}

}  // namespace
}  // namespace browser_controls_api
