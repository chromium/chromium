// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/test/pixel_test_configuration_mixin.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

#if !BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
#error Platform not supported
#endif

// Tests for the chrome://search-engine-choice WebUI page.
namespace {
struct TestParam {
  std::string test_suffix;
  bool use_dark_theme = false;
  bool use_right_to_left_language = false;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

// Permutations of supported parameters.
const TestParam kTestParams[] = {
    {.test_suffix = "Default"},
    {.test_suffix = "DarkTheme", .use_dark_theme = true},
    {.test_suffix = "RightToLeft", .use_right_to_left_language = true},
};
}  // namespace

class SearchEngineChoiceUIPixelTest
    : public TestBrowserDialog,
      public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  SearchEngineChoiceUIPixelTest()
      : scoped_chrome_build_override_(SearchEngineChoiceServiceFactory::
                                          ScopedChromeBuildOverrideForTesting(
                                              /*force_chrome_build=*/true)),
        pixel_test_mixin_(&mixin_host_,
                          GetParam().use_dark_theme,
                          GetParam().use_right_to_left_language) {}

  ~SearchEngineChoiceUIPixelTest() override = default;

  // TestBrowserDialog
  void ShowUi(const std::string& name) override {
    SearchEngineChoiceService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);

    GURL url = GURL(chrome::kChromeUISearchEngineChoiceURL);
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();

    ShowSearchEngineChoiceDialog(*browser());
    observer.Wait();
  }

 private:
  base::AutoReset<bool> scoped_chrome_build_override_;
  base::test::ScopedFeatureList feature_list_{switches::kSearchEngineChoice};
  PixelTestConfigurationMixin pixel_test_mixin_;
};

IN_PROC_BROWSER_TEST_P(SearchEngineChoiceUIPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SearchEngineChoiceUIPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
