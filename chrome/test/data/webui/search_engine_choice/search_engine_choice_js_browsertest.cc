// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"

class SearchEngineChoiceJsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SearchEngineChoiceJsBrowserTest() {
    set_test_loader_host(chrome::kChromeUISearchEngineChoiceHost);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kSearchEngineChoice};
  base::AutoReset<bool> scoped_chrome_build_override_ =
      SearchEngineChoiceServiceFactory::ScopedChromeBuildOverrideForTesting(
          /*force_chrome_build=*/true);
};

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceJsBrowserTest,
                       SearchEngineChoiceTest) {
  RunTest("search_engine_choice/search_engine_choice_test.js", "mocha.run()");
}
