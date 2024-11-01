// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/sanitize_ui/sanitize_ui.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

/**
 * @fileoverview Test suite for chrome://sanitize.
 */

namespace ash {

namespace {

constexpr char foo_url[] = "http://www.foo.com";
constexpr char bar_url[] = "http://bar";

class SanitizeUIBrowserTest : public WebUIMochaBrowserTest {
 public:
  SanitizeUIBrowserTest() {
    set_test_loader_host(::ash::kChromeUISanitizeAppHost);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kSanitize,
                              ash::features::kSanitizeV1},
        /*disabled_featuers=*/{});
  }

 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath =
        base::StringPrintf("chromeos/sanitize_ui/%s", testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }

  // Hook for subclasses that need to perform additional setup steps that
  // involve the WebContents, before the Mocha test runs, in this case
  // preventing the SWA from restarting the Chrome process for testing.
  void OnWebContentsAvailable(content::WebContents* web_contents) override {
    // Set SWA to not restart after completion - harness restarts Chrome process
    // for testing.
    web_contents->GetWebUI()
        ->GetController()
        ->GetAs<SanitizeDialogUI>()
        ->SetAttemptRestartForTesting(base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SanitizeUIBrowserTest, PRE_SanitizeCheckPreferences) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();

  // Ensure user preferences are set to proper test values before safety reset.

  // Homepage settings.
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, foo_url);
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  // Startup page settings.
  const GURL urls[] = {GURL(foo_url), GURL(bar_url)};
  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.assign(urls, urls + std::size(urls));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  // NTP settings.
  auto* ntp_custom_background_service =
      NtpCustomBackgroundServiceFactory::GetForProfile(profile);
  ntp_custom_background_service->AddValidBackdropUrlForTesting(
      GURL("https://background.com"));
  ntp_custom_background_service->SetCustomBackgroundInfo(
      /*background_url=*/GURL("https://background.com"),
      /*thumbnail_url=*/GURL("https://thumbnail.com"),
      /*attribution_line_1=*/"line 1",
      /*attribution_line_2=*/"line 2",
      /*action_url=*/GURL("https://action.com"),
      /*collection_id=*/"");
  EXPECT_TRUE(ntp_custom_background_service->GetCustomBackground().has_value());

  // Proxy settings
  EXPECT_FALSE(prefs->GetBoolean(proxy_config::prefs::kUseSharedProxies));
  prefs->SetBoolean(proxy_config::prefs::kUseSharedProxies, true);

  // Keyboard settings.
  prefs->SetString(prefs::kLanguagePreloadEngines, "xkb:ru::rus");
  EXPECT_NE("en-US", prefs->GetValue(prefs::kLanguagePreloadEngines));

  base::Value::List malicous_values;
  malicous_values.Append("fr");
  malicous_values.Append("es");
  malicous_values.Append("ru");
  prefs->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                 std::move(malicous_values));
  const base::Value::List& spellcheck_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckDictionaries);
  size_t expected_size = 3;
  EXPECT_EQ(spellcheck_dictionaries.size(), expected_size);

  // The Chrome application locale is running on US based profile,
  // where safe keyboard resets are derived from.
  prefs->SetString(language::prefs::kApplicationLocale, "en-US");
  prefs->SetString(language::prefs::kPreferredLanguages, "en-US");

  // Execute Sanitize function, we expect there to be a restart after
  // all resets have been tried.
  RunTestAtPath("sanitize_ui_test.js");
}

IN_PROC_BROWSER_TEST_F(SanitizeUIBrowserTest, SanitizeCheckPreferences) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();

  // Check for expected changes in user preferences.
  // Check homepage resets to expected defaults
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(foo_url, prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowHomeButton));

  // Check startup page preferences to expected defaults.
  const GURL urls[] = {GURL(foo_url), GURL(bar_url)};
  SessionStartupPref startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(), startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(urls, urls + std::size(urls)), startup_pref.urls);

  // Check NTP settings for expected defaults.
  auto* ntp_custom_background_service =
      NtpCustomBackgroundServiceFactory::GetForProfile(profile);
  EXPECT_FALSE(
      ntp_custom_background_service->GetCustomBackground().has_value());

  // Check that "Allow proxies for shared networks" in chrome://settings
  // is disabled.
  EXPECT_FALSE(prefs->GetBoolean(proxy_config::prefs::kUseSharedProxies));

  // Check Keyboard settings for expected defaults.
  // Assume that the session will use the current UI locale.
  std::string locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> input_method_ids;
  ash::input_method::InputMethodManager* manager =
      ash::input_method::InputMethodManager::Get();
  manager->GetInputMethodUtil()->GetInputMethodIdsFromLanguageCode(
      locale, ash::input_method::kAllInputMethods, &input_method_ids);
  ASSERT_FALSE(input_method_ids.empty());
  EXPECT_EQ(input_method_ids[0],
            prefs->GetValue(prefs::kLanguagePreloadEngines));

  std::string expected_language =
      prefs->GetString(language::prefs::kPreferredLanguages);
  const base::Value::List& spellcheck_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckDictionaries);
  size_t expected_size = 1;
  EXPECT_EQ(spellcheck_dictionaries.size(), expected_size);
  EXPECT_THAT(spellcheck_dictionaries,
              ::testing::ElementsAre(expected_language));
}

}  // namespace

}  // namespace ash
