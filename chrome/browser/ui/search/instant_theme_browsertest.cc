// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_utils.h"

class TestNtpThemeObserver : public InstantServiceObserver {
 public:
  explicit TestNtpThemeObserver(InstantService* service) : service_(service) {
    service_->AddObserver(this);
  }

  ~TestNtpThemeObserver() override { service_->RemoveObserver(this); }

  void WaitForThemeApplied(bool theme_installed) {
    DCHECK(!quit_closure_);
    theme_installed_ = theme_installed;
    if (hasThemeInstalled(theme_) == theme_installed_) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void NtpThemeChanged(const NtpTheme& theme) override {
    theme_ = theme;

    if (quit_closure_ && hasThemeInstalled(theme) == theme_installed_) {
      std::move(quit_closure_).Run();
      quit_closure_.Reset();
    }
  }

  void MostVisitedInfoChanged(const InstantMostVisitedInfo&) override {}

  bool hasThemeInstalled(const NtpTheme& theme) { return theme.theme_id != ""; }

  InstantService* const service_;

  NtpTheme theme_;

  bool theme_installed_;
  base::OnceClosure quit_closure_;
};

class InstantThemeTest : public extensions::ExtensionBrowserTest,
                         public InstantTestBase {
 public:
  InstantThemeTest() {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(https_test_server().Start());
    GURL base_url = https_test_server().GetURL("/instant_extended.html");
    GURL ntp_url = https_test_server().GetURL("/instant_extended_ntp.html");
    InstantTestBase::Init(base_url, ntp_url, false);
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    content::URLDataSource::Add(profile(),
                                std::make_unique<ThemeSource>(profile()));
  }

  void InstallThemeAndVerify(const std::string& theme_dir,
                             const std::string& theme_name) {
    bool had_previous_theme =
        !!ThemeServiceFactory::GetThemeForProfile(profile());

    const base::FilePath theme_path = test_data_dir_.AppendASCII(theme_dir);
    // Themes install asynchronously so we must check the number of enabled
    // extensions after theme install completes.
    size_t num_before = extensions::ExtensionRegistry::Get(profile())
                            ->enabled_extensions()
                            .size();
    content::WindowedNotificationObserver theme_change_observer(
        chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
        content::Source<ThemeService>(
            ThemeServiceFactory::GetForProfile(profile())));
    ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(
        theme_path, 1, extensions::ExtensionBrowserTest::browser()));
    theme_change_observer.Wait();
    size_t num_after = extensions::ExtensionRegistry::Get(profile())
                           ->enabled_extensions()
                           .size();
    // If a theme was already installed, we're just swapping one for another, so
    // no change in extension count.
    int expected_change = had_previous_theme ? 0 : 1;
    EXPECT_EQ(num_before + expected_change, num_after);

    const extensions::Extension* new_theme =
        ThemeServiceFactory::GetThemeForProfile(profile());
    ASSERT_NE(nullptr, new_theme);
    ASSERT_EQ(new_theme->name(), theme_name);
  }

  // Loads a named image from |image_url| in the given |tab|. |loaded|
  // returns whether the image was able to load without error.
  // The method returns true if the JavaScript executed cleanly.
  bool LoadImage(content::WebContents* tab,
                 const GURL& image_url,
                 bool* loaded) {
    std::string js_chrome =
        "var img = document.createElement('img');"
        "img.onerror = function() { domAutomationController.send(false); };"
        "img.onload  = function() { domAutomationController.send(true); };"
        "img.src = '" +
        image_url.spec() + "';";
    return content::ExecuteScriptAndExtractBool(tab, js_chrome, loaded);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantThemeTest);
};

IN_PROC_BROWSER_TEST_F(InstantThemeTest, ThemeBackgroundAccess) {
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // The "Instant" New Tab should have access to chrome-search: scheme but not
  // chrome: scheme.
  const GURL chrome_url("chrome://theme/IDR_THEME_NTP_BACKGROUND");
  const GURL search_url("chrome-search://theme/IDR_THEME_NTP_BACKGROUND");
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool loaded = false;
  ASSERT_TRUE(LoadImage(tab, chrome_url, &loaded));
  EXPECT_FALSE(loaded) << chrome_url;
  ASSERT_TRUE(LoadImage(tab, search_url, &loaded));
  EXPECT_TRUE(loaded) << search_url;
}

IN_PROC_BROWSER_TEST_F(InstantThemeTest, ThemeAppliedToExistingTab) {
  // On the existing tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  const std::string helper_js = "document.body.style.cssText";
  TestNtpThemeObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  // Open new tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  observer.WaitForThemeApplied(false);

  // Get the default (no theme) css setting
  std::string original_css_text = "";
  EXPECT_TRUE(instant_test_utils::GetStringFromJS(active_tab, helper_js,
                                                  &original_css_text));

  // Open a new tab and install a theme on the new tab.
  active_tab = local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_EQ(2, browser()->tab_strip_model()->active_index());
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  observer.WaitForThemeApplied(true);

  // Get the current tab's theme CSS setting.
  std::string css_text = "";
  EXPECT_TRUE(
      instant_test_utils::GetStringFromJS(active_tab, helper_js, &css_text));

  // Switch to the previous tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  observer.WaitForThemeApplied(true);

  // Get the previous tab's theme CSS setting.
  std::string previous_tab_css_text = "";
  EXPECT_TRUE(instant_test_utils::GetStringFromJS(active_tab, helper_js,
                                                  &previous_tab_css_text));

  // The previous tab should also apply the new theme.
  EXPECT_NE(original_css_text, css_text);
  EXPECT_EQ(previous_tab_css_text, css_text);
}

IN_PROC_BROWSER_TEST_F(InstantThemeTest, ThemeAppliedToNewTab) {
  // On the existing tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  const std::string helper_js = "document.body.style.cssText";
  TestNtpThemeObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));
  // Open new tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForThemeApplied(false);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Get the default (no theme) css setting
  std::string original_css_text = "";
  EXPECT_TRUE(instant_test_utils::GetStringFromJS(active_tab, helper_js,
                                                  &original_css_text));

  // Install a theme on this tab.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  observer.WaitForThemeApplied(true);

  // Get the current tab's theme CSS setting.
  std::string css_text = "";
  EXPECT_TRUE(
      instant_test_utils::GetStringFromJS(active_tab, helper_js, &css_text));

  // Open a new tab.
  active_tab = local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForThemeApplied(true);
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_EQ(2, browser()->tab_strip_model()->active_index());

  // Get the new tab's theme CSS setting.
  std::string new_tab_css_text = "";
  EXPECT_TRUE(instant_test_utils::GetStringFromJS(active_tab, helper_js,
                                                  &new_tab_css_text));

  // The new tab should change the original theme and also apply the new theme.
  EXPECT_NE(original_css_text, new_tab_css_text);
  EXPECT_EQ(css_text, new_tab_css_text);
}

IN_PROC_BROWSER_TEST_F(InstantThemeTest, ThemeChangedWhenApplyingNewTheme) {
  // On the existing tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  const std::string helper_js = "document.body.style.cssText";
  TestNtpThemeObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  // Open new tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());
  observer.WaitForThemeApplied(false);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Get the default (no theme) css setting
  std::string original_css_text = "";
  EXPECT_TRUE(instant_test_utils::GetStringFromJS(active_tab, helper_js,
                                                  &original_css_text));

  // install a theme on this tab.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  observer.WaitForThemeApplied(true);

  // Get the current tab's theme CSS setting.
  std::string css_text = "";
  EXPECT_TRUE(
      instant_test_utils::GetStringFromJS(active_tab, helper_js, &css_text));

  // Install a different theme.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme2", "snowflake theme"));
  observer.WaitForThemeApplied(true);

  // Get the current tab's theme CSS setting.
  std::string new_css_text = "";
  EXPECT_TRUE(instant_test_utils::GetStringFromJS(active_tab, helper_js,
                                                  &new_css_text));

  // Confirm that the theme will take effect on the current tab when installing
  // a new theme.
  EXPECT_NE(original_css_text, css_text);
  EXPECT_NE(css_text, new_css_text);
  EXPECT_NE(original_css_text, new_css_text);
}
