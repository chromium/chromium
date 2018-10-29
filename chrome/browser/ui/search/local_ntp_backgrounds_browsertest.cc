// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestThemeInfoObserver : public InstantServiceObserver {
 public:
  explicit TestThemeInfoObserver(InstantService* service) : service_(service) {
    service_->AddObserver(this);
  }

  ~TestThemeInfoObserver() override { service_->RemoveObserver(this); }

  void WaitForThemeInfoUpdated(std::string background_url,
                               std::string attribution_1,
                               std::string attribution_2,
                               std::string attribution_action_url) {
    DCHECK(!quit_closure_);

    expected_background_url_ = background_url;
    expected_attribution_1_ = attribution_1;
    expected_attribution_2_ = attribution_2;
    expected_attribution_action_url_ = attribution_action_url;

    if (theme_info_.custom_background_url == background_url &&
        theme_info_.custom_background_attribution_line_1 == attribution_1 &&
        theme_info_.custom_background_attribution_line_2 == attribution_2 &&
        theme_info_.custom_background_attribution_action_url ==
            attribution_action_url) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForThemeApplied(bool theme_installed) {
    DCHECK(!quit_closure_);

    theme_installed_ = theme_installed;
    if (!theme_info_.using_default_theme == theme_installed) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Switch the exit condition for ThemeInfoChanged.
  void SwitchCheck() {
    wait_for_custom_background_or_theme_ =
        !wait_for_custom_background_or_theme_;
  }

  bool IsUsingDefaultTheme() { return theme_info_.using_default_theme; }

 private:
  void ThemeInfoChanged(const ThemeBackgroundInfo& theme_info) override {
    theme_info_ = theme_info;

    if (quit_closure_) {
      // Exit when the custom background was applied successfully.
      if (wait_for_custom_background_or_theme_ &&
          theme_info_.custom_background_url == expected_background_url_ &&
          theme_info_.custom_background_attribution_line_1 ==
              expected_attribution_1_ &&
          theme_info_.custom_background_attribution_line_2 ==
              expected_attribution_2_ &&
          theme_info_.custom_background_attribution_action_url ==
              expected_attribution_action_url_) {
        std::move(quit_closure_).Run();
        quit_closure_.Reset();
      }
      // Exit when the theme was applied successfully.
      else if (!wait_for_custom_background_or_theme_ &&
               !theme_info_.using_default_theme == theme_installed_) {
        std::move(quit_closure_).Run();
        quit_closure_.Reset();
      }
    }
  }

  void MostVisitedItemsChanged(const std::vector<InstantMostVisitedItem>&,
                               bool is_custom_links) override {}

  InstantService* const service_;

  ThemeBackgroundInfo theme_info_;

  bool theme_installed_;
  // When wait_for_custom_background_or_theme_ is true, we wait for the custom
  // background to get applied. When wait_for_custom_background_or_theme_ is
  // false, we wait for a theme gets applied
  bool wait_for_custom_background_or_theme_ = true;
  std::string expected_background_url_;
  std::string expected_attribution_1_;
  std::string expected_attribution_2_;
  std::string expected_attribution_action_url_;
  base::OnceClosure quit_closure_;
};

class LocalNTPCustomBackgroundsTest : public InProcessBrowserTest {
 public:
  LocalNTPCustomBackgroundsTest() {
    feature_list_.InitWithFeatures(
        {features::kUseGoogleLocalNtp, features::kNtpBackgrounds}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest,
                       EmbeddedSearchAPIEndToEnd) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestThemeInfoObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Check that a URL with no attributions can be set.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  instant_service->AddValidBackdropUrlForTesting(GURL("https://www.test.com/"));
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURL('https://www.test.com/"
                                     "')"));
  observer.WaitForThemeInfoUpdated("https://www.test.com/", "", "", "");

  // Check that a URL with attributions can be set.
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURLWithAttributions('https:/"
                                     "/www.test.com/', 'attr1', 'attr2', "
                                     "'https://www.attribution.com/')"));
  observer.WaitForThemeInfoUpdated("https://www.test.com/", "attr1", "attr2",
                                   "https://www.attribution.com/");

  // Setting the background URL to an empty string should clear everything.
  EXPECT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage.setBackgroundURL('')"));
  observer.WaitForThemeInfoUpdated("", "", "", "");
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest, AttributionSetAndReset) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestThemeInfoObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background attribution via the EmbeddedSearch API.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  instant_service->AddValidBackdropUrlForTesting(GURL("https://www.test.com/"));
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURLWithAttributions('https:/"
                                     "/www.test.com/', 'attr1', 'attr2', "
                                     "'https://www.attribution.com/')"));
  observer.WaitForThemeInfoUpdated("https://www.test.com/", "attr1", "attr2",
                                   "https://www.attribution.com/");

  // Check that the custom background element has the correct attribution
  // applied.
  bool result = false;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "document.querySelector('.attr1').innerText === 'attr1' && "
      "document.querySelector('.attr2').innerText === 'attr2'",
      &result));
  EXPECT_TRUE(result);

  // Reset custom background via the EmbeddedSearch API.
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURL('')"));
  observer.WaitForThemeInfoUpdated("", "", "", "");

  // Check that the custom background attribution was cleared.
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('custom-bg-attr').hasChildNodes()", &result));
  EXPECT_FALSE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest,
                       BackgroundImageSetandReset) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestThemeInfoObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  instant_service->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  EXPECT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundURL('chrome-search://local-ntp/background1.jpg"
      "')"));
  observer.WaitForThemeInfoUpdated("chrome-search://local-ntp/background1.jpg",
                                   "", "", "");

  // Check that the custom background element has the correct attribution with
  // the scrim applied.
  bool result = false;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "$('custom-bg').style.backgroundImage == 'linear-gradient(rgba(0, 0, 0, "
      "0), rgba(0, 0, 0, 0.3)), "
      "url(\"chrome-search://local-ntp/background1.jpg\")'",
      &result));
  EXPECT_TRUE(result);

  // Clear the custom background image via the EmbeddedSearch API.
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURL('')"));
  observer.WaitForThemeInfoUpdated("", "", "", "");

  // Check that the custom background was cleared.
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('custom-bg').backgroundImage === undefined", &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest,
                       CustomBackgroundResetAfterChangeDefaultSearchEngine) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestThemeInfoObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  instant_service->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundURL('chrome-search://local-ntp/background1.jpg"
      "')"));
  observer.WaitForThemeInfoUpdated("chrome-search://local-ntp/background1.jpg",
                                   "", "", "");

  // Check that the custom background element has the correct attribution with
  // the scrim applied.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "$('custom-bg').style.backgroundImage == 'linear-gradient(rgba(0, 0, 0, "
      "0), rgba(0, 0, 0, 0.3)), "
      "url(\"chrome-search://local-ntp/background1.jpg\")'",
      &result));
  ASSERT_TRUE(result);

  // Change the default search engine to a non google one
  local_ntp_test_utils::SetUserSelectedDefaultSearchProvider(
      browser()->profile(), "https://www.example.com",
      /*ntp_url=*/"");

  // Open a new blank tab.
  active_tab = local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Navigate to the NTP.
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  bool is_google = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.configData && !!window.configData.isGooglePage",
      &is_google));
  EXPECT_FALSE(is_google);

  // Check that gear icon is not visible.
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('edit-bg').hidden", &result));
  EXPECT_TRUE(result);

  // Check that the custom background image and attributes was cleared.
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('custom-bg').backgroundImage === undefined", &result));
  EXPECT_TRUE(result);
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('custom-bg-attr').hasChildNodes()", &result));
  EXPECT_FALSE(result);
}

class LocalNTPCustomBackgroundsThemeTest
    : public extensions::ExtensionBrowserTest {
 public:
  LocalNTPCustomBackgroundsThemeTest() {
    feature_list_.InitWithFeatures(
        {features::kUseGoogleLocalNtp, features::kNtpBackgrounds}, {});
  }

 protected:
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsThemeTest,
                       KeepGearIconAfterThemeApplied) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestThemeInfoObserver observer(
      InstantServiceFactory::GetForProfile(profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background attribution via the EmbeddedSearch API.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  instant_service->AddValidBackdropUrlForTesting(GURL("https://www.test.com/"));
  ASSERT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURLWithAttributions('https:/"
                                     "/www.test.com/', 'attr1', 'attr2', "
                                     "'https://www.attribution.com/')"));
  observer.WaitForThemeInfoUpdated("https://www.test.com/", "attr1", "attr2",
                                   "https://www.attribution.com/");

  // Check that the custom background element has the correct attribution
  // applied.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "document.querySelector('.attr1').innerText === 'attr1' && "
      "document.querySelector('.attr2').innerText === 'attr2'",
      &result));
  EXPECT_TRUE(result);
  // Apply a custom background still count as using default theme
  ASSERT_TRUE(observer.IsUsingDefaultTheme());

  // Switch to waiting for the theme to get applied.
  observer.SwitchCheck();
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  observer.WaitForThemeApplied(true);
  ASSERT_FALSE(observer.IsUsingDefaultTheme());

  // Check that the custom background attribution is maintained and the
  // user continues to have the option to select a custom background.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "document.querySelector('#edit-bg-default-wallpapers').hidden", &result));
  EXPECT_FALSE(result);

  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('custom-bg-attr').hasChildNodes()", &result));
  EXPECT_TRUE(result);

  // Check that the custom background element maintains the correct attribution.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "document.querySelector('.attr1').innerText === 'attr1' && "
      "document.querySelector('.attr2').innerText === 'attr2'",
      &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsThemeTest,
                       KeepBackgroundImageAfterThemeApplied) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestThemeInfoObserver observer(
      InstantServiceFactory::GetForProfile(profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  instant_service->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundURL('chrome-search://local-ntp/background1.jpg"
      "')"));
  observer.WaitForThemeInfoUpdated("chrome-search://local-ntp/background1.jpg",
                                   "", "", "");

  // Check that the custom background element has the correct attribution with
  // the scrim applied.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "$('custom-bg').style.backgroundImage === 'linear-gradient(rgba(0, 0, 0, "
      "0), rgba(0, 0, 0, 0.3)), "
      "url(\"chrome-search://local-ntp/background1.jpg\")'",
      &result));
  ASSERT_TRUE(result);
  // Apply a custom background still count as using default theme
  ASSERT_TRUE(observer.IsUsingDefaultTheme());

  // Switch to waiting for the theme to get applied.
  observer.SwitchCheck();
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  observer.WaitForThemeApplied(true);
  ASSERT_FALSE(observer.IsUsingDefaultTheme());

  // Check that the custom background image persists after theme
  // is set.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "$('custom-bg').style.backgroundImage === 'linear-gradient(rgba(0, 0, 0, "
      "0), rgba(0, 0, 0, 0.3)), "
      "url(\"chrome-search://local-ntp/background1.jpg\")'",
      &result));
  ASSERT_TRUE(result);
}
}  // namespace
