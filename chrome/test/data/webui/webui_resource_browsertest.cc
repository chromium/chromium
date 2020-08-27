// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/path_service.h"
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

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ArrayDataModelTest) {
  LoadTestUrl("js/cr/ui/array_data_model_test.html");
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

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, I18nProcessCssTest) {
  LoadTestUrl("i18n_process_css_test.html");
}

class WebUIResourceBrowserTestV0 : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Load resources that are only used by browser_tests.
    base::FilePath pak_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &pak_path));
    pak_path = pak_path.AppendASCII("browser_tests.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path, ui::SCALE_FACTOR_NONE);

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(yoichio): This is temporary switch to support chrome internal
    // components migration from the old web APIs.
    // After completion of the migration, we should remove this.
    // See crbug.com/911943 for detail.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "HTMLImports");
  }

  // Runs all test functions in |file|, waiting for them to complete.
  void LoadFile(const std::string& file) {
    GURL test_url =
        embedded_test_server()->GetURL(std::string("/webui/") + file);
    RunTest(test_url);
  }

  // Queues the library corresponding to |resource_id| for injection into the
  // test. The code injection is performed post-load, so any common test
  // initialization that depends on the library should be placed in a setUp
  // function.
  void AddLibrary(int resource_id) {
    include_libraries_.push_back(resource_id);
  }

 private:
  void RunTest(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents, include_libraries_));
  }

  // Resource IDs for internal javascript libraries to inject into the test.
  std::vector<int> include_libraries_;
};

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTestV0, I18nProcessTest) {
  AddLibrary(IDR_WEBUI_JS_LOAD_TIME_DATA);
  AddLibrary(IDR_WEBUI_JS_I18N_TEMPLATE_NO_PROCESS);
  AddLibrary(IDR_WEBUI_JS_UTIL);
  LoadFile("i18n_process_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ListTest) {
  LoadTestUrl("js/cr/ui/list_test.html");
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, GridTest) {
  LoadTestUrl("js/cr/ui/grid_test.html");
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ListSelectionModelTest) {
  LoadTestUrl("js/cr/ui/list_selection_model_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ListSingleSelectionModelTest) {
  LoadTestUrl("js/cr/ui/list_single_selection_model_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MenuTest) {
  LoadTestUrl("js/cr/ui/menu_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MockTimerTest) {
  LoadTestUrl("mock_timer_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ParseHtmlSubsetTest) {
  LoadTestUrl("parse_html_subset_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, PositionUtilTest) {
  LoadTestUrl("js/cr/ui/position_util_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, CommandTest) {
  LoadTestUrl("js/cr/ui/command_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, ContextMenuHandlerTest) {
  LoadTestUrl("js/cr/ui/context_menu_handler_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, MenuButtonTest) {
  LoadTestUrl("js/cr/ui/menu_button_test.html");
}

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, SplitterTest) {
  LoadTestUrl("js/cr/ui/splitter_test.html");
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

IN_PROC_BROWSER_TEST_F(WebUIResourceBrowserTest, I18nBehaviorTest) {
  LoadTestUrl("i18n_behavior_test.html");
}
