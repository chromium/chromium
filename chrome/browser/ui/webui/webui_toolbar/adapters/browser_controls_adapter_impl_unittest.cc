
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter_impl.h"

#include <memory>

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

// Matches `content::OpenURLParams` that open `url` in the active tab, which is
// how the adapter handles every drop on the toolbar.
MATCHER_P(OpensInCurrentTab, url, "") {
  return arg.url == url &&
         arg.disposition == WindowOpenDisposition::CURRENT_TAB;
}

// Matches `content::OpenURLParams` whose initiator is an opaque origin, as set
// for drags that started in a web page so the navigation is not treated as
// browser-initiated (b/563340726).
MATCHER(HasOpaqueInitiator, "") {
  return arg.initiator_origin.has_value() && arg.initiator_origin->opaque();
}

// Matches `content::OpenURLParams` without an initiator, as expected for drags
// that started outside the browser.
MATCHER(HasNoInitiator, "") {
  return !arg.initiator_origin.has_value();
}

TEST_F(BrowserControlsAdapterImplTest, NavigateText_HTTPAllowed) {
  GURL expected_url("https://www.example.com/");
  EXPECT_CALL(browser_window_interface_,
              OpenURL(OpensInCurrentTab(expected_url), _));
  adapter_->NavigateText("https://www.example.com/");
}

TEST_F(BrowserControlsAdapterImplTest,
       NavigateText_PrivilegedSchemeBlockedOnRendererDrag) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(true);
  EXPECT_CALL(browser_window_interface_, OpenURL(_, _)).Times(0);
  adapter_->NavigateText("chrome://settings");
}

// The adapter trusts the drag state as recorded. ChromeOS treating every drag
// as renderer-originated (b/256022714) happens earlier, in
// `PreHandleDragUpdate`, which these tests bypass.
TEST_F(BrowserControlsAdapterImplTest,
       NavigateText_PrivilegedSchemeWhenDragNotRendererTainted) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(false);
  EXPECT_CALL(browser_window_interface_,
              OpenURL(OpensInCurrentTab(GURL("chrome://settings")), _));
  adapter_->NavigateText("chrome://settings");
}

TEST_F(BrowserControlsAdapterImplTest,
       Navigate_PrivilegedSchemeWhenDragNotRendererTainted) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(false);
  EXPECT_CALL(browser_window_interface_,
              OpenURL(OpensInCurrentTab(GURL("chrome://settings")), _));
  adapter_->Navigate(GURL("chrome://settings"));
}

TEST_F(BrowserControlsAdapterImplTest,
       Navigate_PrivilegedSchemeIgnoredOnRendererDrag) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(true);
  EXPECT_CALL(browser_window_interface_, OpenURL(_, _)).Times(0);
  adapter_->Navigate(GURL("chrome://settings"));
}

TEST_F(BrowserControlsAdapterImplTest,
       Navigate_IgnoredWhenUnfilteredDragUrlIsJavaScript) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_has_javascript_url(true);
  EXPECT_CALL(browser_window_interface_, OpenURL(_, _)).Times(0);
  adapter_->Navigate(GURL("about:blank#blocked"));
}

TEST_F(BrowserControlsAdapterImplTest,
       Navigate_RendererDragHasOpaqueInitiator) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(true);
  const GURL url("https://www.example.com/");
  EXPECT_CALL(
      browser_window_interface_,
      OpenURL(::testing::AllOf(OpensInCurrentTab(url), HasOpaqueInitiator()),
              _));
  adapter_->Navigate(url);
}

TEST_F(BrowserControlsAdapterImplTest,
       NavigateText_RendererDragHasOpaqueInitiator) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(true);
  const GURL url("https://www.example.com/");
  EXPECT_CALL(
      browser_window_interface_,
      OpenURL(::testing::AllOf(OpensInCurrentTab(url), HasOpaqueInitiator()),
              _));
  adapter_->NavigateText(url.spec());
}

TEST_F(BrowserControlsAdapterImplTest, Navigate_OsDragHasNoInitiator) {
  webui_toolbar::WebUIToolbarDragState::GetOrCreateForWebContents(
      web_contents())
      ->set_drag_originated_from_renderer(false);
  const GURL url("https://www.example.com/");
  EXPECT_CALL(
      browser_window_interface_,
      OpenURL(::testing::AllOf(OpensInCurrentTab(url), HasNoInitiator()), _));
  adapter_->Navigate(url);
}

}  // namespace
}  // namespace browser_controls_api
