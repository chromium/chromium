// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"

class SearchEngineChoiceBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SearchEngineChoiceBrowserTest() {
    set_test_loader_host(chrome::kChromeUISearchEngineChoiceHost);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry,
                                    switches::kDefaultListCountryOverride);
    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
    command_line->AppendSwitch(switches::kForceSearchEngineChoiceScreen);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUp() override {
    scoped_chrome_build_override_ = std::make_unique<base::AutoReset<bool>>(
        SearchEngineChoiceDialogServiceFactory::
            ScopedChromeBuildOverrideForTesting(
                /*force_chrome_build=*/true));
    WebUIMochaBrowserTest::SetUp();
  }

 private:
  std::unique_ptr<base::AutoReset<bool>> scoped_chrome_build_override_;
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kFirstRunDesktopRefresh};
};

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest, AppRefresh) {
  RunTest("search_engine_choice/app_refresh_test.js", "mocha.run()");
}
