// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest CrComponentsFocusTest;

IN_PROC_BROWSER_TEST_F(CrComponentsFocusTest, MostVisited) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/most_visited_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsFocusTest, CrShortcutInput) {
  RunTest("cr_components/cr_shortcut_input/cr_shortcut_input_test.js",
          "mocha.run()");
}

class CrComponentsHistoryClustersFocusTest : public WebUIMochaFocusTest {
 protected:
  CrComponentsHistoryClustersFocusTest() {
    scoped_feature_list_.InitAndEnableFeature(
        history_clusters::internal::kJourneysImages);
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersFocusTest, All) {
  RunTest("cr_components/history_clusters/history_clusters_test.js",
          "runMochaSuite('HistoryClustersFocusTest')");
}
