// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/system_web_app_manager_browsertest.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace web_app {

namespace {

class SystemWebAppManagerBrowserTestChromeos
    : public SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppManagerBrowserTestChromeos()
      : SystemWebAppManagerBrowserTest(false /* install_mock */) {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kHelpAppV2, chromeos::features::kMediaApp}, {});
  }

  // Runs basic tests on a System Web App. E.g. ensures it exists, and
  // loads/navigates with an expected title that matches the manifest app name.
  void ExpectSystemWebAppValid(SystemAppType app_type,
                               const GURL& url,
                               const std::string& title) {
    Browser* app_browser = WaitForSystemAppInstallAndLaunch(app_type);
    const extensions::Extension* installed_app =
        extensions::util::GetInstalledPwaForUrl(browser()->profile(), url);

    EXPECT_TRUE(GetManager().IsSystemWebApp(installed_app->id()));
    EXPECT_TRUE(installed_app->from_bookmark());

    EXPECT_EQ(title, installed_app->name());
    EXPECT_EQ(base::ASCIIToUTF16(title),
              app_browser->window()->GetNativeWindow()->GetTitle());
    EXPECT_EQ(extensions::Manifest::EXTERNAL_COMPONENT,
              installed_app->location());

    // The installed app should match the opened app window.
    EXPECT_EQ(installed_app, GetExtensionForAppBrowser(app_browser));
    content::WebContents* web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();

    // The opened window should be showing the url with attached WebUI.
    EXPECT_EQ(url, web_contents->GetVisibleURL());

    content::TestNavigationObserver observer(web_contents);
    observer.WaitForNavigationFinished();
    EXPECT_EQ(url, web_contents->GetLastCommittedURL());

    content::WebUI* web_ui = web_contents->GetCommittedWebUI();
    ASSERT_TRUE(web_ui);
    EXPECT_TRUE(web_ui->GetController());

    // A completed navigation could change the window title. Check again.
    EXPECT_EQ(base::ASCIIToUTF16(title),
              app_browser->window()->GetNativeWindow()->GetTitle());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Test that the Media App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerBrowserTestChromeos, MediaApp) {
  const GURL* url = new GURL(chromeos::kChromeUIMediaAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(SystemAppType::MEDIA, *url, "Media App"));
}

// Test that the Help App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerBrowserTestChromeos, HelpAppV2) {
  const GURL* url = new GURL(chromeos::kChromeUIHelpAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(SystemAppType::HELP, *url, "Help App"));
}

}  // namespace web_app
