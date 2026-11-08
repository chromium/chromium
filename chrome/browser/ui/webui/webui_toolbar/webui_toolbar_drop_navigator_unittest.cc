// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_drop_navigator.h"

#include <memory>

#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_drag_state.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "url/gurl.h"

namespace webui_toolbar {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class WebUIToolbarDropNavigatorTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUIToolbarDropNavigatorTest() = default;

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
    navigator_ = std::make_unique<WebUIToolbarDropNavigator>(
        browser_window_interface_, web_contents());
  }

  void TearDown() override {
    navigator_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<WebUIToolbarDropNavigator> navigator_;
};

// Matches `content::OpenURLParams` that open `url` in the active tab, which is
// how the navigator handles every drop on the toolbar.
MATCHER_P(OpensInCurrentTab, url, "") {
  return arg.url == url &&
         arg.disposition == ui::mojom::WindowOpenDisposition::kCurrentTab;
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

TEST_F(WebUIToolbarDropNavigatorTest, NavigateToDroppedText_HTTPAllowed) {
  GURL expected_url("https://www.example.com/");
  EXPECT_CALL(browser_window_interface_,
              OpenURL(OpensInCurrentTab(expected_url), _));
  navigator_->NavigateToDroppedText("https://www.example.com/");
}

TEST_F(WebUIToolbarDropNavigatorTest,
       NavigateToDroppedText_PrivilegedSchemeBlockedOnRendererDrag) {
  WebUIToolbarDragState::GetOrCreateForWebContents(web_contents())
      ->set_drag_originated_from_renderer(true);
  EXPECT_CALL(browser_window_interface_, OpenURL(_, _)).Times(0);
  navigator_->NavigateToDroppedText("chrome://settings");
}

// The navigator trusts the drag state as recorded. ChromeOS treating every drag
// as renderer-originated (b/256022714) happens earlier, in
// `PreHandleDragUpdate`, which these tests bypass.
TEST_F(WebUIToolbarDropNavigatorTest,
       NavigateToDroppedText_PrivilegedSchemeWhenDragNotRendererTainted) {
  WebUIToolbarDragState::GetOrCreateForWebContents(web_contents())
      ->set_drag_originated_from_renderer(false);
  EXPECT_CALL(browser_window_interface_,
              OpenURL(OpensInCurrentTab(GURL("chrome://settings")), _));
  navigator_->NavigateToDroppedText("chrome://settings");
}

TEST_F(WebUIToolbarDropNavigatorTest,
       NavigateToDroppedUrl_PrivilegedSchemeWhenDragNotRendererTainted) {
  WebUIToolbarDragState::GetOrCreateForWebContents(web_contents())
      ->set_drag_originated_from_renderer(false);
  EXPECT_CALL(browser_window_interface_,
              OpenURL(OpensInCurrentTab(GURL("chrome://settings")), _));
  navigator_->NavigateToDroppedUrl(GURL("chrome://settings"));
}

TEST_F(WebUIToolbarDropNavigatorTest,
       NavigateToDroppedUrl_PrivilegedSchemeIgnoredOnRendererDrag) {
  WebUIToolbarDragState::GetOrCreateForWebContents(web_contents())
      ->set_drag_originated_from_renderer(true);
  EXPECT_CALL(browser_window_interface_, OpenURL(_, _)).Times(0);
  navigator_->NavigateToDroppedUrl(GURL("chrome://settings"));
}

TEST_F(WebUIToolbarDropNavigatorTest, NavigateToDroppedUrl_JavaScriptIgnored) {
  EXPECT_CALL(browser_window_interface_, OpenURL(_, _)).Times(0);
  navigator_->NavigateToDroppedUrl(GURL("javascript:alert(1)"));
}

TEST_F(WebUIToolbarDropNavigatorTest,
       NavigateToDroppedUrl_RendererDragHasOpaqueInitiator) {
  WebUIToolbarDragState::GetOrCreateForWebContents(web_contents())
      ->set_drag_originated_from_renderer(true);
  const GURL url("https://www.example.com/");
  EXPECT_CALL(
      browser_window_interface_,
      OpenURL(::testing::AllOf(OpensInCurrentTab(url), HasOpaqueInitiator()),
              _));
  navigator_->NavigateToDroppedUrl(url);
}

TEST_F(WebUIToolbarDropNavigatorTest,
       NavigateToDroppedText_RendererDragHasOpaqueInitiator) {
  WebUIToolbarDragState::GetOrCreateForWebContents(web_contents())
      ->set_drag_originated_from_renderer(true);
  const GURL url("https://www.example.com/");
  EXPECT_CALL(
      browser_window_interface_,
      OpenURL(::testing::AllOf(OpensInCurrentTab(url), HasOpaqueInitiator()),
              _));
  navigator_->NavigateToDroppedText(url.spec());
}

TEST_F(WebUIToolbarDropNavigatorTest,
       NavigateToDroppedUrl_OsDragHasNoInitiator) {
  WebUIToolbarDragState::GetOrCreateForWebContents(web_contents())
      ->set_drag_originated_from_renderer(false);
  const GURL url("https://www.example.com/");
  EXPECT_CALL(
      browser_window_interface_,
      OpenURL(::testing::AllOf(OpensInCurrentTab(url), HasNoInitiator()), _));
  navigator_->NavigateToDroppedUrl(url);
}

}  // namespace
}  // namespace webui_toolbar
