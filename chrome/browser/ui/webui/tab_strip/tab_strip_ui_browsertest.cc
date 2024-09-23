// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"

#include <memory>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

class MockTabStripUIEmbedder : public TabStripUIEmbedder {
 public:
  MOCK_CONST_METHOD0(GetAcceleratorProvider, const ui::AcceleratorProvider*());
  MOCK_METHOD(void, CloseContainer, ());
  MOCK_METHOD(void,
              ShowContextMenuAtPoint,
              (gfx::Point,
               std::unique_ptr<ui::MenuModel>,
               base::RepeatingClosure));
  MOCK_METHOD(void, CloseContextMenu, ());
  MOCK_METHOD(void,
              ShowEditDialogForGroupAtPoint,
              (gfx::Point, gfx::Rect, tab_groups::TabGroupId));
  MOCK_METHOD(void, HideEditDialogForGroup, ());
  MOCK_METHOD(TabStripUILayout, GetLayout, ());
  MOCK_CONST_METHOD1(GetColorProviderColor, SkColor(ui::ColorId));
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

// https://crbug.com/1246369: Test is flaky on Linux/Windows, disabled for
// investigation.
// https://crbug.com/1263485: Also flaky on chromeos.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ActivatingTabClosesEmbedder DISABLED_ActivatingTabClosesEmbedder
#else
#define MAYBE_ActivatingTabClosesEmbedder ActivatingTabClosesEmbedder
#endif

IN_PROC_BROWSER_TEST_F(TabStripUIBrowserTest,
                       MAYBE_ActivatingTabClosesEmbedder) {
  const std::string activate_tab_js = tab_query_js + ".click()";

  EXPECT_CALL(mock_embedder_, CloseContainer()).Times(1);
  ASSERT_TRUE(content::ExecJs(webui_contents_.get(), activate_tab_js,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL));
}

IN_PROC_BROWSER_TEST_F(TabStripUIBrowserTest, InvokesEditDialogForGroups) {
  using ::testing::_;

  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  // Wait for the front-end to receive the new group and create the tab-group
  // element.
  const std::string get_group_promise_js =
      "new Promise((resolve) => {"
      "  const interval = setInterval(() => {"
      "    if (document.querySelector('tabstrip-tab-list').shadowRoot"
      "        .querySelector('tabstrip-tab-group')) {"
      "      resolve(true);"
      "      clearInterval(interval);"
      "    }"
      "  }, 100);"
      "});";
  ASSERT_TRUE(content::EvalJs(webui_contents_.get(), get_group_promise_js,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL)
                  .ExtractBool());

  const std::string get_chip_js =
      "const chip = document.querySelector('tabstrip-tab-list')"
      "    .shadowRoot.querySelector('tabstrip-tab-group')"
      "    .shadowRoot.querySelector('#chip');"
      "const chipRect = chip.getBoundingClientRect();";
  int left =
      content::EvalJs(webui_contents_.get(), get_chip_js + "chipRect.left",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                      ISOLATED_WORLD_ID_CHROME_INTERNAL)
          .ExtractInt();
  int top = content::EvalJs(webui_contents_.get(), get_chip_js + "chipRect.top",
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                            ISOLATED_WORLD_ID_CHROME_INTERNAL)
                .ExtractInt();
  int width =
      content::EvalJs(webui_contents_.get(), get_chip_js + "chipRect.width",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                      ISOLATED_WORLD_ID_CHROME_INTERNAL)
          .ExtractInt();
  int height =
      content::EvalJs(webui_contents_.get(), get_chip_js + "chipRect.height",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                      ISOLATED_WORLD_ID_CHROME_INTERNAL)
          .ExtractInt();

  EXPECT_CALL(mock_embedder_,
              ShowEditDialogForGroupAtPoint(gfx::Point(left, top),
                                            gfx::Rect(width, height), group_id))
      .Times(1);
  ASSERT_TRUE(content::ExecJs(webui_contents_.get(),
                              get_chip_js + "chip.click();",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL));
}
