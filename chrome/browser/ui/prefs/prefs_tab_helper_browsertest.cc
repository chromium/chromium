// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/prefs/prefs_tab_helper.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"

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

// Tests that Arabic Nastaliq font family preferences are registered and
// populated with platform-specific default values from GRD resources.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, ArabicNastaliqDefaultPrefs) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();

  // Verify the Arabic Nastaliq prefs are registered.
  const PrefService::Preference* standard_pref =
      prefs->FindPreference(prefs::kWebKitStandardFontFamilyArabicNastaliq);
  const PrefService::Preference* serif_pref =
      prefs->FindPreference(prefs::kWebKitSerifFontFamilyArabicNastaliq);
  const PrefService::Preference* sans_serif_pref =
      prefs->FindPreference(prefs::kWebKitSansSerifFontFamilyArabicNastaliq);

  ASSERT_TRUE(standard_pref);
  ASSERT_TRUE(serif_pref);
  ASSERT_TRUE(sans_serif_pref);

  // All three should still be at their default values (not overridden by user).
  EXPECT_TRUE(standard_pref->IsDefaultValue());
  EXPECT_TRUE(serif_pref->IsDefaultValue());
  EXPECT_TRUE(sans_serif_pref->IsDefaultValue());

  // The defaults should be non-empty (populated from GRD resources).
  EXPECT_FALSE(
      prefs->GetString(prefs::kWebKitStandardFontFamilyArabicNastaliq).empty());
  EXPECT_FALSE(
      prefs->GetString(prefs::kWebKitSerifFontFamilyArabicNastaliq).empty());
  EXPECT_FALSE(prefs->GetString(prefs::kWebKitSansSerifFontFamilyArabicNastaliq)
                   .empty());
}

// Tests that Arabic Nastaliq font family preferences propagate correctly to
// Blink's WebPreferences font family maps under the "Aran" script key.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, ArabicNastaliqFontFamilies) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamilyArabicNastaliq,
                   "CustomAranStandard");
  prefs->SetString(prefs::kWebKitSerifFontFamilyArabicNastaliq,
                   "CustomAranSerif");
  prefs->SetString(prefs::kWebKitSansSerifFontFamilyArabicNastaliq,
                   "CustomAranSansSerif");

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  web_contents->NotifyPreferencesChanged();
  blink::web_pref::WebPreferences web_prefs =
      web_contents->GetOrCreateWebPreferences();

  EXPECT_EQ(u"CustomAranStandard", web_prefs.standard_font_family_map["Aran"]);
  EXPECT_EQ(u"CustomAranSerif", web_prefs.serif_font_family_map["Aran"]);
  EXPECT_EQ(u"CustomAranSansSerif",
            web_prefs.sans_serif_font_family_map["Aran"]);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
class PrefsTabHelperDeferredFontListBrowserTest
    : public PrefsTabHelperBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PrefsTabHelperDeferredFontListBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kDeferScriptFontPrefResolution, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that per-script comma-separated font family default lists remain
// unresolved in `PrefService` when `features::kDeferScriptFontPrefResolution`
// is enabled (or are resolved eagerly when disabled) and propagate to Blink's
// `WebPreferences`.
IN_PROC_BROWSER_TEST_P(PrefsTabHelperDeferredFontListBrowserTest,
                       PerScriptFontListResolution) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  const std::string common_serif =
      prefs->GetString(prefs::kWebKitSerifFontFamily);
  ASSERT_FALSE(common_serif.empty());
  EXPECT_FALSE(common_serif.starts_with(','));

  const std::string simplified_han_sans_serif =
      prefs->GetString(prefs::kWebKitSansSerifFontFamilySimplifiedHan);
  ASSERT_FALSE(simplified_han_sans_serif.empty());
  EXPECT_EQ(GetParam(), simplified_han_sans_serif.starts_with(','));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  blink::web_pref::WebPreferences web_prefs =
      web_contents->GetOrCreateWebPreferences();
  EXPECT_EQ(base::UTF8ToUTF16(simplified_han_sans_serif),
            web_prefs.sans_serif_font_family_map["Hans"]);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrefsTabHelperDeferredFontListBrowserTest,
                         testing::Bool());
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#endif  // !BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

// On Android, `FontSelector::FamilyNameFromSettings` in Blink ignores the font
// family maps from `WebPreferences`.
#if !BUILDFLAG(IS_ANDROID)

namespace {

// Returns the primary platform font family used by Blink to render `<body>` in
// `web_contents`, queried via DevTools `CSS.getPlatformFontsForNode`.
std::string GetFirstPlatformFontForBody(
    content::TestDevToolsProtocolClient& devtools_client,
    content::WebContents* web_contents) {
  // Force a synchronous style and layout update so
  // `CSS.getPlatformFontsForNode` reads up-to-date shaped text runs from
  // `<body>`'s `LayoutObject`.
  EXPECT_TRUE(content::ExecJs(web_contents, "document.body.offsetTop"));

  // Retrieve the root document's DevTools `nodeId`.
  base::DictValue doc_params;
  doc_params.Set("depth", 0);
  const base::DictValue* doc_result =
      devtools_client.SendCommand("DOM.getDocument", std::move(doc_params));
  CHECK(doc_result);
  std::optional<int> root_node_id =
      doc_result->FindIntByDottedPath("root.nodeId");
  CHECK(root_node_id);

  // Look up the `<body>` element's DevTools `nodeId` from the document root.
  base::DictValue query_params;
  query_params.Set("nodeId", *root_node_id);
  query_params.Set("selector", "body");
  const base::DictValue* query_result =
      devtools_client.SendCommand("DOM.querySelector", std::move(query_params));
  CHECK(query_result);
  std::optional<int> body_node_id = query_result->FindInt("nodeId");
  CHECK(body_node_id);

  // Query the platform fonts used by Blink to shape text inside `<body>`.
  base::DictValue font_params;
  font_params.Set("nodeId", *body_node_id);
  const base::DictValue* font_info = devtools_client.SendCommand(
      "CSS.getPlatformFontsForNode", std::move(font_params));
  CHECK(font_info);
  const base::ListValue* font_list = font_info->FindList("fonts");
  CHECK(font_list);
  CHECK(!font_list->empty());
  const std::string* first_font_name =
      font_list->front().GetDict().FindString("familyName");
  CHECK(first_font_name);
  return *first_font_name;
}

}  // namespace

// Verifies end-to-end that generic and per-script font family preferences in
// `PrefService` flow through `FontFamilyCache` and `PrefsTabHelper` into Blink
// and select the expected platform font during layout.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest,
                       FontFamilyPreferencesEndToEndRendering) {
  // `CSS.getPlatformFontsForNode` returns the matched system typeface name: OS
  // fonts on Win/Mac vs hermetic `third_party/test_fonts` on Linux/ChromeOS.
#if BUILDFLAG(IS_WIN)
  const std::string kCustomFont = "Lucida Console";
  const std::string kExpectedSansSerifFont = "Arial";
#elif BUILDFLAG(IS_MAC)
  const std::string kCustomFont = "Monaco";
  const std::string kExpectedSansSerifFont = "Helvetica";
#else
  const std::string kCustomFont = "Ahem";
  const std::string kExpectedSansSerifFont = "Arimo";
#endif

  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  // Clear the user-store overrides loaded by `SetUpUserDataDirectory()` so all
  // preferences start at their registered platform defaults.
  prefs->ClearPref(prefs::kWebKitStandardFontFamily);
  prefs->ClearPref(prefs::kWebKitSansSerifFontFamily);

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      GURL("data:text/html;charset=utf-8,<body lang='en'>A</body>")));

  content::TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(web_contents);
  ASSERT_TRUE(devtools_client.SendCommandSync("DOM.enable"));
  ASSERT_TRUE(devtools_client.SendCommandSync("CSS.enable"));

  struct GenericFamilyTestCase {
    const char* css_family;
    const char* pref_name;
    const char* expected_default_font;
  };
  constexpr GenericFamilyTestCase kTestCases[] = {
#if BUILDFLAG(IS_WIN)
      {"initial", prefs::kWebKitStandardFontFamily, "Times New Roman"},
      {"serif", prefs::kWebKitSerifFontFamily, "Times New Roman"},
      {"sans-serif", prefs::kWebKitSansSerifFontFamily, "Arial"},
      {"cursive", prefs::kWebKitCursiveFontFamily, "Comic Sans MS"},
      {"fantasy", prefs::kWebKitFantasyFontFamily, "Impact"},
      // `PrefsTabHelper::RegisterProfilePrefs` replaces `IDS_FIXED_FONT_FAMILY`
      // ("Courier New") with `IDS_FIXED_FONT_FAMILY_ALT_WIN` ("Consolas").
      {"monospace", prefs::kWebKitFixedFontFamily, "Consolas"},
      {"math", prefs::kWebKitMathFontFamily, "Cambria Math"},
#elif BUILDFLAG(IS_MAC)
      {"initial", prefs::kWebKitStandardFontFamily, "Times"},
      {"serif", prefs::kWebKitSerifFontFamily, "Times"},
      {"sans-serif", prefs::kWebKitSansSerifFontFamily, "Helvetica"},
      {"cursive", prefs::kWebKitCursiveFontFamily, "Apple Chancery"},
      {"fantasy", prefs::kWebKitFantasyFontFamily, "Papyrus"},
      {"monospace", prefs::kWebKitFixedFontFamily, "Menlo"},
      {"math", prefs::kWebKitMathFontFamily, "STIX Two Math"},
#else
      // On Linux/ChromeOS, `third_party/test_fonts` aliases standard/serif to
      // "Tinos", sans-serif to "Arimo", and monospace to "Cousine"; cursive,
      // fantasy, and math defaults are not in `test_fonts` and fall back to
      // standard ("Tinos").
      {"initial", prefs::kWebKitStandardFontFamily, "Tinos"},
      {"serif", prefs::kWebKitSerifFontFamily, "Tinos"},
      {"sans-serif", prefs::kWebKitSansSerifFontFamily, "Arimo"},
      {"cursive", prefs::kWebKitCursiveFontFamily, "Tinos"},
      {"fantasy", prefs::kWebKitFantasyFontFamily, "Tinos"},
      {"monospace", prefs::kWebKitFixedFontFamily, "Cousine"},
      {"math", prefs::kWebKitMathFontFamily, "Tinos"},
#endif
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.css_family);
    ASSERT_TRUE(content::ExecJs(
        web_contents, base::StrCat({"document.body.style.fontFamily = '",
                                    test_case.css_family, "'"})));

    EXPECT_TRUE(base::test::RunUntil([&]() {
      return GetFirstPlatformFontForBody(devtools_client, web_contents) ==
             test_case.expected_default_font;
    }));

    // Updating the preference in `PrefService` updates `FontFamilyCache` and
    // triggers `PrefsTabHelper::OnWebPrefChanged` (via a posted task).
    prefs->SetString(test_case.pref_name, kCustomFont);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return GetFirstPlatformFontForBody(devtools_client, web_contents) ==
             kCustomFont;
    }));

    prefs->ClearPref(test_case.pref_name);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return GetFirstPlatformFontForBody(devtools_client, web_contents) ==
             test_case.expected_default_font;
    }));
  }

  // Because the page has `<body lang='en'>`, Blink resolves `sans-serif` by
  // looking up the Latin script ("Latn") in `sans_serif_font_family_map` first,
  // falling back to the script-independent Common entry ("Zyyy", i.e.
  // `kWebKitSansSerifFontFamily`) only if "Latn" is unset.
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "document.body.style.fontFamily = 'sans-serif'"));

  // Setting the Latin ("Latn") sans-serif preference overrides the Common
  // ("Zyyy") sans-serif font (`kExpectedSansSerifFont`) for `<body lang='en'>`.
  prefs->SetString("webkit.webprefs.fonts.sansserif.Latn", kCustomFont);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetFirstPlatformFontForBody(devtools_client, web_contents) ==
           kCustomFont;
  }));

  // Setting the Latin ("Latn") preference to "" removes the "Latn" entry from
  // `sans_serif_font_family_map`, so Blink falls back to the Common ("Zyyy")
  // sans-serif font (`kExpectedSansSerifFont`) again.
  prefs->SetString("webkit.webprefs.fonts.sansserif.Latn", std::string());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetFirstPlatformFontForBody(devtools_client, web_contents) ==
           kExpectedSansSerifFont;
  }));

  devtools_client.DetachProtocolClient();
}
#endif  // !BUILDFLAG(IS_ANDROID)
