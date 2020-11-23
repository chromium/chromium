// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_browsertest_base.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using LocalNTPCustomBackgroundsTest = InProcessBrowserTest;

class TestInstantService {
 public:
  explicit TestInstantService(Profile* profile) {
    instant_service = InstantServiceFactory::GetForProfile(profile);
    instant_service->SetImageFetcherForTesting(
        new testing::NiceMock<image_fetcher::MockImageFetcher>());
  }
  InstantService* get_instant_service() { return instant_service; }

 private:
  InstantService* instant_service;
};

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest,
                       EmbeddedSearchAPIEndToEnd) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Check that a URL with no attributions can be set.
  TestInstantService test_instant_service(browser()->profile());
  test_instant_service.get_instant_service()->AddValidBackdropUrlForTesting(
      GURL("https://www.test.com/"));
  EXPECT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundInfo('https://www.test.com/', '', '', '', '')"));

  observer.WaitForNtpThemeUpdated("https://www.test.com/", "", "", "");

  // Check that a URL with attributions can be set.
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundInfo('https:/"
                                     "/www.test.com/', 'attr1', 'attr2', "
                                     "'https://www.attribution.com/', '')"));
  observer.WaitForNtpThemeUpdated("https://www.test.com/", "attr1", "attr2",
                                  "https://www.attribution.com/");

  // Setting the background URL to an empty string should clear everything.
  EXPECT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage.resetBackgroundInfo()"));
  observer.WaitForNtpThemeUpdated("", "", "", "");
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest, AttributionSetAndReset) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background attribution via the EmbeddedSearch API.
  TestInstantService test_instant_service(browser()->profile());
  test_instant_service.get_instant_service()->AddValidBackdropUrlForTesting(
      GURL("https://www.test.com/"));
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundInfo('https:/"
                                     "/www.test.com/', 'attr1', 'attr2', "
                                     "'https://www.attribution.com/', '')"));
  observer.WaitForNtpThemeUpdated("https://www.test.com/", "attr1", "attr2",
                                  "https://www.attribution.com/");

  // Check that the custom background element has the correct attribution
  // applied.
  bool result = false;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "document.querySelector('#attr1').innerText === 'attr1' && "
      "document.querySelector('#attr2').innerText === 'attr2'",
      &result));
  EXPECT_TRUE(result);

  // Reset custom background via the EmbeddedSearch API.
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "resetBackgroundInfo()"));
  observer.WaitForNtpThemeUpdated("", "", "", "");

  // Check that the custom background attribution was cleared.
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('custom-bg-attr').hasChildNodes()", &result));
  EXPECT_FALSE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest,
                       BackgroundImageSetandReset) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  TestInstantService test_instant_service(browser()->profile());
  test_instant_service.get_instant_service()->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  EXPECT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundInfo('chrome-search://local-ntp/background1.jpg"
      "', '', '' ,'' ,'')"));
  observer.WaitForNtpThemeUpdated("chrome-search://local-ntp/background1.jpg",
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
                                     "resetBackgroundInfo()"));
  observer.WaitForNtpThemeUpdated("", "", "", "");

  // Check that the custom background was cleared.
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('custom-bg').backgroundImage === undefined", &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest,
                       CustomBackgroundResetAfterChangeDefaultSearchEngine) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  TestInstantService test_instant_service(browser()->profile());
  test_instant_service.get_instant_service()->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundInfo('chrome-search://local-ntp/background1.jpg"
      "', '', '', '', '')"));
  observer.WaitForNtpThemeUpdated("chrome-search://local-ntp/background1.jpg",
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
  void SetUp() override {
    ON_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(policy_provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    extensions::ExtensionBrowserTest::SetUp();
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

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsThemeTest,
                       KeepGearIconAfterThemeApplied) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background attribution via the EmbeddedSearch API.
  TestInstantService test_instant_service(browser()->profile());
  test_instant_service.get_instant_service()->AddValidBackdropUrlForTesting(
      GURL("https://www.test.com/"));
  ASSERT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundInfo('https:/"
                                     "/www.test.com/', 'attr1', 'attr2', "
                                     "'https://www.attribution.com/', '')"));
  observer.WaitForNtpThemeUpdated("https://www.test.com/", "attr1", "attr2",
                                  "https://www.attribution.com/");

  // Check that the custom background element has the correct attribution
  // applied.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "document.querySelector('#attr1').innerText === 'attr1' && "
      "document.querySelector('#attr2').innerText === 'attr2'",
      &result));
  EXPECT_TRUE(result);
  // Apply a custom background still count as using default theme
  ASSERT_TRUE(observer.IsUsingDefaultTheme());

  // Switch to waiting for the theme to get applied.
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
      "document.querySelector('#attr1').innerText === 'attr1' && "
      "document.querySelector('#attr2').innerText === 'attr2'",
      &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsThemeTest,
                       KeepBackgroundImageAfterThemeApplied) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  TestInstantService test_instant_service(browser()->profile());
  test_instant_service.get_instant_service()->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundInfo('chrome-search://local-ntp/background1.jpg"
      "', '', '', '', '')"));
  observer.WaitForNtpThemeUpdated("chrome-search://local-ntp/background1.jpg",
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

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsThemeTest,
                       CustomBackgroundOverridesThemeAttribution) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Switch to waiting for the theme to get applied.
  ASSERT_NO_FATAL_FAILURE(
      InstallThemeAndVerify("theme_with_attribution", "attribution theme"));
  observer.WaitForThemeApplied(true);
  EXPECT_FALSE(observer.IsUsingDefaultTheme());
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage.ntpTheme."
      "attributionUrl !== ''",
      &result));
  EXPECT_TRUE(result);

  // Set a custom background image via the EmbeddedSearch API.
  TestInstantService test_instant_service(browser()->profile());
  test_instant_service.get_instant_service()->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundInfo('chrome-search://local-ntp/background1.jpg"
      "', '', '', '', '')"));
  observer.WaitForNtpThemeUpdated("chrome-search://local-ntp/background1.jpg",
                                  "", "", "");

  // Check that the custom background element has the correct attribution with
  // the scrim applied.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "$('custom-bg').style.backgroundImage === 'linear-gradient(rgba(0, 0, 0, "
      "0), rgba(0, 0, 0, 0.3)), "
      "url(\"chrome-search://local-ntp/background1.jpg\")'",
      &result));
  EXPECT_TRUE(result);
  // Applying a custom background still counts as using the default theme.
  EXPECT_FALSE(observer.IsUsingDefaultTheme());
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage.ntpTheme."
      "attributionUrl === ''",
      &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsThemeTest,
                       CustomBackgroundEnabledPolicy) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  TestInstantServiceObserver observer(instant_service);

  // Set a custom background image via the EmbeddedSearch API.
  instant_service->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  EXPECT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundInfo('chrome-search://local-ntp/background1.jpg"
      "', '', '' ,'' ,'')"));
  observer.WaitForNtpThemeUpdated("chrome-search://local-ntp/background1.jpg",
                                  "", "", "");
  EXPECT_FALSE(instant_service->IsCustomBackgroundDisabledByPolicy());

  policy::PolicyMap policies1;
  policies1.Set(policy::key::kNTPCustomBackgroundEnabled,
                policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy_provider_.UpdateChromePolicy(policies1);
  base::RunLoop().RunUntilIdle();

  // Make sure setting the policy to false clears the background and prevents
  // setting one in the UI.
  observer.WaitForNtpThemeUpdated("", "", "", "");
  EXPECT_TRUE(observer.IsCustomBackgroundDisabledByPolicy());

  policy::PolicyMap policies2;
  policies2.Set(policy::key::kNTPCustomBackgroundEnabled,
                policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy_provider_.UpdateChromePolicy(policies2);
  base::RunLoop().RunUntilIdle();

  // Make sure setting the policy to true does not restore the background yet
  // allows setting one in the UI.
  observer.WaitForNtpThemeUpdated("", "", "", "");
  EXPECT_FALSE(observer.IsCustomBackgroundDisabledByPolicy());

  policy::PolicyMap policies3;
  policy_provider_.UpdateChromePolicy(policies3);
  base::RunLoop().RunUntilIdle();

  // Make sure clearing the policy to true does not restore the background yet
  // allows setting one in the UI.
  observer.WaitForNtpThemeUpdated("", "", "", "");
  EXPECT_FALSE(observer.IsCustomBackgroundDisabledByPolicy());
}

// TODO(crbug/980638): Update/Remove when Linux and/or ChromeOS support dark
// mode.
#if defined(OS_WIN) || defined(OS_MAC)

// Tests that dark mode styling is properly applied when a theme and/or custom
// background is set.
class LocalNTPBackgroundsAndDarkModeTest
    : public LocalNTPCustomBackgroundsThemeTest,
      public DarkModeTestBase {
 public:
  LocalNTPBackgroundsAndDarkModeTest() {}

 protected:
  void SetUpOnMainThread() override {
    LocalNTPCustomBackgroundsThemeTest::SetUpOnMainThread();

    theme()->AddColorSchemeNativeThemeObserver(
        ui::NativeTheme::GetInstanceForWeb());

    // Enable dark mode.
    instant_service =
        InstantServiceFactory::GetForProfile(browser()->profile());
    theme()->SetDarkMode(true);
    instant_service->SetNativeThemeForTesting(theme());
    theme()->NotifyObservers();
    instant_service->SetImageFetcherForTesting(
        new testing::NiceMock<image_fetcher::MockImageFetcher>());
  }

  InstantService* instant_service;
};

IN_PROC_BROWSER_TEST_F(LocalNTPBackgroundsAndDarkModeTest,
                       WithCustomBackground) {
  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(profile()));
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  instant_service->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundInfo('chrome-search://local-ntp/background1.jpg"
      "', '', '', '', '')"));
  observer.WaitForNtpThemeUpdated("chrome-search://local-ntp/background1.jpg",
                                  "", "", "");

  // Elements other than chips (i.e. Most Visited, etc.) should have dark mode
  // applied.
  EXPECT_TRUE(GetIsDarkModeApplied(active_tab));
  EXPECT_TRUE(GetIsLightChipsApplied(active_tab));
}

IN_PROC_BROWSER_TEST_F(LocalNTPBackgroundsAndDarkModeTest, WithTheme) {
  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(profile()));
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Switch to waiting for the theme to get applied. With img
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  observer.WaitForThemeApplied(true);
  ASSERT_FALSE(observer.IsUsingDefaultTheme());

  // Elements other than chips (i.e. Most Visited, etc.) should have dark mode
  // applied.
  EXPECT_TRUE(GetIsDarkModeApplied(active_tab));
  EXPECT_TRUE(GetIsLightChipsApplied(active_tab));
}

IN_PROC_BROWSER_TEST_F(LocalNTPBackgroundsAndDarkModeTest, WithThemeNoImage) {
  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(profile()));
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Switch to waiting for the theme to get applied. With img
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme_minimal", "minimal"));
  observer.WaitForThemeApplied(true);
  ASSERT_FALSE(observer.IsUsingDefaultTheme());

  // All elements should have dark mode applied.
  EXPECT_TRUE(GetIsDarkModeApplied(active_tab));
  EXPECT_FALSE(GetIsLightChipsApplied(active_tab));
}

IN_PROC_BROWSER_TEST_F(LocalNTPBackgroundsAndDarkModeTest,
                       WithThemeAndCustomBackground) {
  TestInstantServiceObserver observer(
      InstantServiceFactory::GetForProfile(profile()));
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  instant_service->AddValidBackdropUrlForTesting(
      GURL("chrome-search://local-ntp/background1.jpg"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundInfo('chrome-search://local-ntp/background1.jpg',"
      "'', '', '', '')"));
  observer.WaitForNtpThemeUpdated("chrome-search://local-ntp/background1.jpg",
                                  "", "", "");

  // Switch to waiting for the theme to get applied. With img
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme_minimal", "minimal"));
  observer.WaitForThemeApplied(true);
  ASSERT_FALSE(observer.IsUsingDefaultTheme());

  // Elements other than chips (i.e. Most Visited, etc.) should have dark mode
  // applied.
  EXPECT_TRUE(GetIsDarkModeApplied(active_tab));
  EXPECT_TRUE(GetIsLightChipsApplied(active_tab));
}

#endif
