// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaBrowserTest CrComponentsCustomizeColorSchemeModeTest;
IN_PROC_BROWSER_TEST_F(CrComponentsCustomizeColorSchemeModeTest, All) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/customize_color_scheme_mode_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsCustomizeThemesTest;
IN_PROC_BROWSER_TEST_F(CrComponentsCustomizeThemesTest, All) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/customize_themes_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsHelpBubbleMixinTest;
IN_PROC_BROWSER_TEST_F(CrComponentsHelpBubbleMixinTest, All) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/help_bubble_mixin_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsHelpBubbleTest;
IN_PROC_BROWSER_TEST_F(CrComponentsHelpBubbleTest, All) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/help_bubble_test.js", "mocha.run()");
}

class CrComponentsHistoryClustersTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsHistoryClustersTest() {
    scoped_feature_list_.InitAndEnableFeature(
        history_clusters::internal::kJourneysImages);
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersTest, All) {
  RunTest("cr_components/history_clusters_test.js", "mocha.run()");
}

class CrComponentsMostVisitedTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsMostVisitedTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, General) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('General');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, Layouts) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Layouts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ReflowLayouts) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Reflow Layouts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, LoggingAndUpdates) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('LoggingAndUpdates');");
}

// crbug.com/1226996
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_Modification DISABLED_Modification
#else
#define MAYBE_Modification Modification
#endif
IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, MAYBE_Modification) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Modification');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, DragAndDrop) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('DragAndDrop');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, Theming) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Theming');");
}
