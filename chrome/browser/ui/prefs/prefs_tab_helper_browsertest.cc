// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace {

base::FilePath GetPreferencesFilePath() {
  base::FilePath test_data_directory;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
  return test_data_directory.AppendASCII("profiles")
      .AppendASCII("web_prefs")
      .AppendASCII("Default")
      .Append(chrome::kPreferencesFilename);
}

}  // namespace

class PrefsTabHelperBrowserTest : public PlatformBrowserTest {
 protected:
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
      LOG(ERROR) << "Copy error from " << pref_file.MaybeAsASCII() << " to "
                 << default_pref_file.MaybeAsASCII();
      return false;
    }

#if BUILDFLAG(IS_WIN)
    // Make the copy writable.  On POSIX we assume the umask allows files
    // we create to be writable.
    if (!::SetFileAttributesW(default_pref_file.value().c_str(),
                              FILE_ATTRIBUTE_NORMAL)) {
      return false;
    }
#endif
    return true;
  }
};

// Tests that a sampling of web prefs are registered and ones with values in the
// test user preferences file take on those values.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, WebPrefs) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();

  EXPECT_TRUE(
      prefs->FindPreference(prefs::kWebKitCursiveFontFamily)->IsDefaultValue());
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kWebKitSerifFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(prefs::kWebKitSerifFontFamilyJapanese)
                  ->IsDefaultValue());

  EXPECT_EQ("windows-1251", prefs->GetString(prefs::kDefaultCharset));
  EXPECT_EQ(16, prefs->GetInteger(prefs::kWebKitDefaultFontSize));
  EXPECT_EQ("Noto Sans CJK KR",
            prefs->GetString(prefs::kWebKitStandardFontFamilyKorean));
  EXPECT_EQ("Tinos", prefs->GetString(prefs::kWebKitStandardFontFamily));
  EXPECT_EQ("DejaVu Sans", prefs->GetString(prefs::kWebKitSansSerifFontFamily));
}

// Tests that changes in browser preferences are reflected in Blink's web
// preferences. Note that these preferences are not handled on non-desktop
// Android, see http://crbug.com/40337093, but can be modified by extension APIs
// on desktop Android.
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, GenericFontFamilies) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamily, "CustomStandard");
  prefs->SetString(prefs::kWebKitSerifFontFamily, "CustomSerif");
  prefs->SetString(prefs::kWebKitSansSerifFontFamily, "CustomSansSerif");
  prefs->SetString(prefs::kWebKitCursiveFontFamily, "CustomCursive");
  prefs->SetString(prefs::kWebKitFantasyFontFamily, "CustomFantasy");
  prefs->SetString(prefs::kWebKitFixedFontFamily, "CustomFixed");
  prefs->SetString(prefs::kWebKitMathFontFamily, "CustomMath");

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
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

// Tests that Devanagari font family preferences are registered and populated
// with platform-specific default values from GRD resources.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, DevanagariDefaultPrefs) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();

  // Verify the Devanagari prefs are registered.
  const PrefService::Preference* standard_pref =
      prefs->FindPreference(prefs::kWebKitStandardFontFamilyDevanagari);
  const PrefService::Preference* fixed_pref =
      prefs->FindPreference(prefs::kWebKitFixedFontFamilyDevanagari);
  const PrefService::Preference* serif_pref =
      prefs->FindPreference(prefs::kWebKitSerifFontFamilyDevanagari);
  const PrefService::Preference* sans_serif_pref =
      prefs->FindPreference(prefs::kWebKitSansSerifFontFamilyDevanagari);

  ASSERT_TRUE(standard_pref);
  ASSERT_TRUE(fixed_pref);
  ASSERT_TRUE(serif_pref);
  ASSERT_TRUE(sans_serif_pref);

  // All four should still be at their default values (not overridden by user).
  EXPECT_TRUE(standard_pref->IsDefaultValue());
  EXPECT_TRUE(fixed_pref->IsDefaultValue());
  EXPECT_TRUE(serif_pref->IsDefaultValue());
  EXPECT_TRUE(sans_serif_pref->IsDefaultValue());

  // The defaults should be non-empty (populated from GRD resources).
  EXPECT_FALSE(
      prefs->GetString(prefs::kWebKitStandardFontFamilyDevanagari).empty());
  EXPECT_FALSE(
      prefs->GetString(prefs::kWebKitFixedFontFamilyDevanagari).empty());
  EXPECT_FALSE(
      prefs->GetString(prefs::kWebKitSerifFontFamilyDevanagari).empty());
  EXPECT_FALSE(
      prefs->GetString(prefs::kWebKitSansSerifFontFamilyDevanagari).empty());
}

// Tests that Devanagari font family preferences propagate correctly to Blink's
// WebPreferences font family maps under the "Deva" script key.
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, DevanagariFontFamilies) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamilyDevanagari,
                   "CustomDevaStandard");
  prefs->SetString(prefs::kWebKitFixedFontFamilyDevanagari, "CustomDevaFixed");
  prefs->SetString(prefs::kWebKitSerifFontFamilyDevanagari, "CustomDevaSerif");
  prefs->SetString(prefs::kWebKitSansSerifFontFamilyDevanagari,
                   "CustomDevaSansSerif");

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  web_contents->NotifyPreferencesChanged();
  blink::web_pref::WebPreferences web_prefs =
      web_contents->GetOrCreateWebPreferences();

  EXPECT_EQ(u"CustomDevaStandard", web_prefs.standard_font_family_map["Deva"]);
  EXPECT_EQ(u"CustomDevaFixed", web_prefs.fixed_font_family_map["Deva"]);
  EXPECT_EQ(u"CustomDevaSerif", web_prefs.serif_font_family_map["Deva"]);
  EXPECT_EQ(u"CustomDevaSansSerif",
            web_prefs.sans_serif_font_family_map["Deva"]);
}
#endif
