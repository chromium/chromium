// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

class PrefsTabHelperBrowserTest : public InProcessBrowserTest {
 protected:
  virtual base::FilePath GetPreferencesFilePath() {
    base::FilePath test_data_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    return test_data_directory
        .AppendASCII("profiles")
        .AppendASCII("web_prefs")
        .AppendASCII("Default")
        .Append(chrome::kPreferencesFilename);
  }

  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_directory;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    base::FilePath default_profile =
        user_data_directory.AppendASCII(TestingProfile::kTestUserProfileDir);
    if (!base::CreateDirectory(default_profile)) {
      LOG(ERROR) << "Can't create " << default_profile.MaybeAsASCII();
      return false;
    }
    base::FilePath pref_file = GetPreferencesFilePath();
    if (!base::PathExists(pref_file)) {
      LOG(ERROR) << "Doesn't exist " << pref_file.MaybeAsASCII();
      return false;
    }
    base::FilePath default_pref_file =
        default_profile.Append(chrome::kPreferencesFilename);
    if (!base::CopyFile(pref_file, default_pref_file)) {
      LOG(ERROR) << "Copy error from " << pref_file.MaybeAsASCII()
                 << " to " << default_pref_file.MaybeAsASCII();
      return false;
    }

#if BUILDFLAG(IS_WIN)
    // Make the copy writable.  On POSIX we assume the umask allows files
    // we create to be writable.
    if (!::SetFileAttributesW(default_pref_file.value().c_str(),
                              FILE_ATTRIBUTE_NORMAL)) return false;
#endif
    return true;
  }
};

// Tests that a sampling of web prefs are registered and ones with values in the
// test user preferences file take on those values.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, WebPrefs) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitCursiveFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitSerifFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitSerifFontFamilyJapanese)->IsDefaultValue());

  EXPECT_EQ("windows-1251", prefs->GetString(prefs::kDefaultCharset));
  EXPECT_EQ(16, prefs->GetInteger(prefs::kWebKitDefaultFontSize));
  EXPECT_EQ("Noto Sans CJK KR",
            prefs->GetString(prefs::kWebKitStandardFontFamilyKorean));
  EXPECT_EQ("Tinos", prefs->GetString(prefs::kWebKitStandardFontFamily));
  EXPECT_EQ("DejaVu Sans", prefs->GetString(prefs::kWebKitSansSerifFontFamily));
}

// Tests that changes in browser preferences are reflected in Blink's web
// preferences. Note that these preferences are not handled on Android, see
// http://crbug.com/308033.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, GenericFontFamilies) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamily, "CustomStandard");
  prefs->SetString(prefs::kWebKitSerifFontFamily, "CustomSerif");
  prefs->SetString(prefs::kWebKitSansSerifFontFamily, "CustomSansSerif");
  prefs->SetString(prefs::kWebKitCursiveFontFamily, "CustomCursive");
  prefs->SetString(prefs::kWebKitFantasyFontFamily, "CustomFantasy");
  prefs->SetString(prefs::kWebKitFixedFontFamily, "CustomFixed");
  prefs->SetString(prefs::kWebKitMathFontFamily, "CustomMath");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->NotifyPreferencesChanged();
  blink::web_pref::WebPreferences web_prefs =
      web_contents->GetOrCreateWebPreferences();

  EXPECT_EQ(u"CustomStandard",
            web_prefs.standard_font_family_map[blink::web_pref::kCommonScript]);
  EXPECT_EQ(u"CustomSerif",
            web_prefs.serif_font_family_map[blink::web_pref::kCommonScript]);
  EXPECT_EQ(
      u"CustomSansSerif",
      web_prefs.sans_serif_font_family_map[blink::web_pref::kCommonScript]);
  EXPECT_EQ(u"CustomCursive",
            web_prefs.cursive_font_family_map[blink::web_pref::kCommonScript]);
  EXPECT_EQ(u"CustomFantasy",
            web_prefs.fantasy_font_family_map[blink::web_pref::kCommonScript]);
  EXPECT_EQ(u"CustomFixed",
            web_prefs.fixed_font_family_map[blink::web_pref::kCommonScript]);
  EXPECT_EQ(u"CustomMath",
            web_prefs.math_font_family_map[blink::web_pref::kCommonScript]);
}
#endif
