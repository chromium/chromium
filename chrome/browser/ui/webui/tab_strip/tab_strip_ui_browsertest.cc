// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

class MockTabStripUIEmbedder : public TabStripUI::Embedder {
 public:
  MOCK_METHOD(void, CloseContainer, (), (override));
  MOCK_METHOD(void,
              ShowContextMenuAtPoint,
              (gfx::Point point, std::unique_ptr<ui::MenuModel> menu_model),
              (override));
  MOCK_METHOD(TabStripUILayout, GetLayout, (), (override));
};

}  // namespace

class TabStripUIBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    // In this test, we create our own TabStripUI instance with a mock
    // Embedder. Disable the production one to avoid conflicting with
    // it.
    feature_override_.InitAndDisableFeature(features::kWebUITabStrip);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    feature_override_.Reset();
  }

  void SetUpOnMainThread() override {
    const TabStripUILayout default_layout =
        TabStripUILayout::CalculateForWebViewportSize(gfx::Size(200, 200));
    ON_CALL(mock_embedder_, GetLayout())
        .WillByDefault(::testing::Return(default_layout));

    webui_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));

    // Start loading WebUI, injecting our TabStripUI::Embedder immediately
    // after.
    webui_contents_->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(
            GURL(chrome::kChromeUITabStripURL)));
    TabStripUI* const tab_strip_ui =
        static_cast<TabStripUI*>(webui_contents_->GetWebUI()->GetController());
    tab_strip_ui->Initialize(browser(), &mock_embedder_);

    // Finish loading after initializing TabStripUI.
    ASSERT_TRUE(content::WaitForLoadStop(webui_contents_.get()));
  }

  void TearDownOnMainThread() override { webui_contents_.reset(); }

 protected:
  static const std::string tab_query_js;

  ::testing::NiceMock<MockTabStripUIEmbedder> mock_embedder_;
  std::unique_ptr<content::WebContents> webui_contents_;

 private:
  base::test::ScopedFeatureList feature_override_;
};

// static
const std::string TabStripUIBrowserTest::tab_query_js(
    "document.querySelector('tabstrip-tab-list')"
    "    .shadowRoot.querySelector('tabstrip-tab')"
    "    .shadowRoot.querySelector('#tab')");

IN_PROC_BROWSER_TEST_F(TabStripUIBrowserTest, ActivatingTabClosesEmbedder) {
  const std::string activate_tab_js = tab_query_js + ".click()";

  EXPECT_CALL(mock_embedder_, CloseContainer()).Times(1);
  ASSERT_TRUE(content::ExecJs(webui_contents_.get(), activate_tab_js,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL));
}

// Checks that the contextmenu event on a tab gets forwarded to the
// TabStripUI::Embedder.
IN_PROC_BROWSER_TEST_F(TabStripUIBrowserTest,
                       InvokesEmbedderContextMenuForTab) {
  using ::testing::_;

  const std::string invoke_menu_js =
      "const event ="
      "    new MouseEvent('contextmenu', { clientX: 100, clientY: 50 });" +
      tab_query_js + ".dispatchEvent(event)";

  EXPECT_CALL(mock_embedder_, ShowContextMenuAtPoint(gfx::Point(100, 50), _))
      .Times(1);
  ASSERT_TRUE(content::ExecJs(webui_contents_.get(), invoke_menu_js,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL));
}
