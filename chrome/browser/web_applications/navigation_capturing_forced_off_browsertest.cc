// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {
enum class NavCaptureOffConfig {
  kFeatureOn,
  kFeatureOnInitialNavOff,
  kFeatureOnUserSettingOff
};

std::string NavCaptureToString(
    const testing::TestParamInfo<NavCaptureOffConfig>& info) {
  switch (info.param) {
    case NavCaptureOffConfig::kFeatureOn:
      return "FeatureOn";
    case NavCaptureOffConfig::kFeatureOnInitialNavOff:
      return "FeatureOnInitialNavOff";
    case NavCaptureOffConfig::kFeatureOnUserSettingOff:
      return "FeatureOnUserSettingOff";
  }
}
class NavigationCapturingForcedOffTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<NavCaptureOffConfig> {
 public:
  constexpr static std::string_view kTestAppUrl =
      "https://www.example.com/app/";

  NavigationCapturingForcedOffTest() {
    if (GetParam() == NavCaptureOffConfig::kFeatureOn) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kPwaNavigationCapturing,
          {{"link_capturing_state", "reimpl_default_on"}});
    } else {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kPwaNavigationCapturing,
          {{"link_capturing_state", "reimpl_default_on"},
           {GetParam() == NavCaptureOffConfig::kFeatureOnInitialNavOff
                ? "initial_nav_forced_off_apps"
                : "user_settings_forced_off_apps",
            GenerateAppIdFromManifestId(GURL(kTestAppUrl))}});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(NavigationCapturingForcedOffTest, CheckBehavior) {
  webapps::AppId app_id =
      test::InstallDummyWebApp(profile(), "App", GURL(kTestAppUrl));
  ASSERT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_id),
            base::ok());

  content::WebContents* browser_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    ui_test_utils::UrlLoadObserver url_observer((GURL(kTestAppUrl)));
    std::string script = content::JsReplace(
        R"(window.open($1, '_blank', 'noopener');)", kTestAppUrl);
    EXPECT_TRUE(content::ExecJs(browser_tab, script));
    url_observer.Wait();
    content::WebContents* contents = url_observer.web_contents();

    // Wait for & get the web contents that was loaded.
    bool expected_open_in_app_window =
        GetParam() == NavCaptureOffConfig::kFeatureOn;
    EXPECT_EQ(expected_open_in_app_window,
              WebAppTabHelper::FromWebContents(contents)->is_in_app_window());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         NavigationCapturingForcedOffTest,
                         testing::Values(
#if !BUILDFLAG(IS_CHROMEOS)
                             NavCaptureOffConfig::kFeatureOnUserSettingOff,
#endif
                             NavCaptureOffConfig::kFeatureOn,
                             NavCaptureOffConfig::kFeatureOnInitialNavOff),
                         NavCaptureToString);
}  // namespace
}  // namespace web_app
