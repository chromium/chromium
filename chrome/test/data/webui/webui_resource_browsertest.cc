// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/test_data_source.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/webui_resources.h"

class WebUIResourceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Setup chrome://test/ data source.
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
    content::URLDataSource::Add(profile,
                                std::make_unique<TestDataSource>("webui"));
  }

  void LoadTestUrl(const std::string& file) {
    GURL url(std::string("chrome://test/") + file);
    RunTest(url);
  }

 private:
  void RunTest(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents, {}));
  }
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ArrayDataModelTest) {
  LoadTestUrl("js/cr/ui/array_data_model_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ArrayDataModelModuleTest) {
  LoadTestUrl("?module=js/cr/ui/array_data_model_test.m.js");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, CrTest) {
  LoadTestUrl("cr_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, CrReloadTest) {
  LoadTestUrl("cr_reload_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, EventTargetTest) {
  LoadTestUrl("js/cr/event_target_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, EventTargetModuleTest) {
  LoadTestUrl("?module=js/cr/event_target_test.m.js");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, I18nProcessCssTest) {
  LoadTestUrl("i18n_process_css_test.html");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ListTest) {
  LoadTestUrl("js/cr/ui/list_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ListModuleTest) {
  LoadTestUrl("?module=js/cr/ui/list_test.m.js");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, GridTest) {
  LoadTestUrl("js/cr/ui/grid_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, GridModuleTest) {
  LoadTestUrl("?module=js/cr/ui/grid_test.m.js");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ListSelectionModelTest) {
  LoadTestUrl("js/cr/ui/list_selection_model_test.html");
}

// This test is Chrome OS only as the utils file it imports relies on
// list_single_selection_model, which is only included on Chrome OS.
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ListSelectionModelModuleTest) {
  LoadTestUrl("?module=js/cr/ui/list_selection_model_test.m.js");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ListSingleSelectionModelTest) {
  LoadTestUrl("js/cr/ui/list_single_selection_model_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest,
                       ListSingleSelectionModelModuleTest) {
  LoadTestUrl("?module=js/cr/ui/list_single_selection_model_test.m.js");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MenuTest) {
  LoadTestUrl("js/cr/ui/menu_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MenuModuleTest) {
  LoadTestUrl("?module=js/cr/ui/menu_test.m.js");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MockTimerTest) {
  LoadTestUrl("mock_timer_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ParseHtmlSubsetTest) {
  LoadTestUrl("parse_html_subset_test.html");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, PositionUtilTest) {
  LoadTestUrl("js/cr/ui/position_util_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, PositionUtilModuleTest) {
  LoadTestUrl("?module=js/cr/ui/position_util_test.m.js");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, CommandTest) {
  LoadTestUrl("js/cr/ui/command_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, CommandModuleTest) {
  LoadTestUrl("?module=js/cr/ui/command_test.m.js");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ContextMenuHandlerTest) {
  LoadTestUrl("js/cr/ui/context_menu_handler_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ContextMenuHandlerModuleTest) {
  LoadTestUrl("?module=js/cr/ui/context_menu_handler_test.m.js");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MenuButtonTest) {
  LoadTestUrl("js/cr/ui/menu_button_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MenuButtonModuleTest) {
  LoadTestUrl("?module=js/cr/ui/menu_button_test.m.js");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, SplitterTest) {
  LoadTestUrl("js/cr/ui/splitter_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, SplitterModuleTest) {
  LoadTestUrl("?module=js/cr/ui/splitter_test.m.js");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, UtilTest) {
  LoadTestUrl("util_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, IconTest) {
  LoadTestUrl("js/icon_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, PromiseResolverTest) {
  LoadTestUrl("promise_resolver_test.html");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, I18nBehaviorTest) {
  LoadTestUrl("i18n_behavior_test.html");
}
#endif
