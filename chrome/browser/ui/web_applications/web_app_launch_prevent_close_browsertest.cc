// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/browser_test.h"

namespace web_app {

constexpr char kTestApp[] = "https://test.test/";

class PreventCloseControllerBrowserTest : public WebAppControllerBrowserTest {
 public:
  PreventCloseControllerBrowserTest() { BuildAndInitFeatureList(); }

 protected:
  void BuildAndInitFeatureList() {
    scoped_feature_list_.InitWithFeatures({features::kDesktopPWAsRunOnOsLogin,
                                           features::kDesktopPWAsPreventClose},
                                          {});
  }

  void ApplyPolicySettings(const GURL& url, bool prevent_close) {
    profile()->GetPrefs()->SetList(
        prefs::kWebAppSettings,
        base::Value::List().Append(base::Value::Dict()
                                       .Set(kManifestId, url.spec())
                                       .Set(kRunOnOsLogin, kRunWindowed)
                                       .Set(kPreventClose, prevent_close)));
  }

  void ClearPolicySettings() {
    profile()->GetPrefs()->SetList(prefs::kWebAppSettings, base::Value::List());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PreventCloseControllerBrowserTest,
                       NonClosablePWADoesNotLaunchAdditionalWindow) {
  // Arrange with non-closable PWA and start it a first time
  size_t expected_browser_count = chrome::GetBrowserCount(profile());

  const GURL url(kTestApp);
  ApplyPolicySettings(url, /*prevent_close=*/true);
  const AppId& app_id = InstallPWA(GURL(kTestApp));

  Browser* browser = LaunchWebAppBrowser(app_id);
  ++expected_browser_count;

  ASSERT_TRUE(browser);
  EXPECT_EQ(expected_browser_count, chrome::GetBrowserCount(profile()));

  // Act by launching PWA a second time
  Browser* second_browser = LaunchWebAppBrowser(app_id);

#if BUILDFLAG(IS_CHROMEOS)
  // Assert that the PWA only has one existing window
  EXPECT_TRUE(WebAppProvider::GetForTest(profile())
                  ->registrar_unsafe()
                  .IsPreventCloseEnabled(app_id));
  EXPECT_EQ(browser, second_browser);
  EXPECT_EQ(expected_browser_count, chrome::GetBrowserCount(profile()));
#else
  // On other platforms, the prevent close should not be enabled.
  EXPECT_FALSE(WebAppProvider::GetForTest(profile())
                   ->registrar_unsafe()
                   .IsPreventCloseEnabled(app_id));
  EXPECT_NE(browser, second_browser);
  EXPECT_EQ(expected_browser_count + 1, chrome::GetBrowserCount(profile()));
#endif

  ClearPolicySettings();
}

IN_PROC_BROWSER_TEST_F(PreventCloseControllerBrowserTest,
                       ClosablePWALaunchesAdditionalWindow) {
  size_t expected_browser_count = chrome::GetBrowserCount(profile());

  const GURL url(kTestApp);
  const AppId& app_id = InstallPWA(url);
  ApplyPolicySettings(url, /*prevent_close=*/false);
  Browser* browser = LaunchWebAppBrowser(app_id);
  ++expected_browser_count;

  ASSERT_TRUE(browser);
  EXPECT_EQ(expected_browser_count, chrome::GetBrowserCount(profile()));

  // Act by launching PWA a second time
  Browser* second_browser = LaunchWebAppBrowser(app_id);
  expected_browser_count++;

  // Assert that the PWA only has one existing window
  EXPECT_NE(browser, second_browser);
  EXPECT_EQ(expected_browser_count, chrome::GetBrowserCount(profile()));

  ClearPolicySettings();
}

}  // namespace web_app
