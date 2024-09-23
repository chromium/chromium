// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/prefs/prefs_tab_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/font_pref_change_notifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/prefs/pref_watcher.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_font_webkit_names.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_names_util.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/overlay_user_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/strings/grit/components_locale_settings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/browser_ui/accessibility/android/font_size_prefs_android.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// If a font name in prefs default values starts with a comma, consider it's a
// comma-separated font list and resolve it to the first available font.
#define PREFS_FONT_LIST 1
#include "ui/gfx/font_list.h"
#else
#define PREFS_FONT_LIST 0
#endif

using blink::web_pref::WebPreferences;
using content::WebContents;

namespace {

#if !BUILDFLAG(IS_ANDROID)
// Registers a preference under the path |pref_name| for each script used for
// per-script font prefs.
// For example, for WEBKIT_WEBPREFS_FONTS_SERIF ("fonts.serif"):
// "fonts.serif.Arab", "fonts.serif.Hang", etc. are registered.
// |fonts_with_defaults| contains all |pref_names| already registered since they
// have a specified default value.
// On Android there are no default values for these properties and there is no
// way to set them (because extensions are not supported so the Font Settings
// API cannot be used), so we can avoid registering them altogether.
void RegisterFontFamilyPrefs(user_prefs::PrefRegistrySyncable* registry,
                             const std::set<std::string>& fonts_with_defaults) {
  // Expand the font concatenated with script name so this stays at RO memory
  // rather than allocated in heap.
  // clang-format off
  static const char* const kFontFamilyMap[] = {
#define EXPAND_SCRIPT_FONT(map_name, script_name) map_name "." script_name,

#include "chrome/common/pref_font_script_names-inl.h"
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_CURSIVE)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_FANTASY)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_FIXED)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_MATH)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_SANSERIF)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_SERIF)
ALL_FONT_SCRIPTS(WEBKIT_WEBPREFS_FONTS_STANDARD)

#undef EXPAND_SCRIPT_FONT
  };
  // clang-format on

  for (size_t i = 0; i < std::size(kFontFamilyMap); ++i) {
    const char* pref_name = kFontFamilyMap[i];
    if (fonts_with_defaults.find(pref_name) == fonts_with_defaults.end()) {
      // We haven't already set a default value for this font preference, so set
      // an empty string as the default.
      registry->RegisterStringPref(pref_name, std::string());
    }
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
// On Windows with antialiasing we want to use an alternate fixed font like
// Consolas, which looks much better than Courier New.
bool ShouldUseAlternateDefaultFixedFont(const std::string& script) {
  if (!base::StartsWith(script, "courier",
                        base::CompareCase::INSENSITIVE_ASCII))
    return false;
  UINT smooth_type = 0;
  SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smooth_type, 0);
  return smooth_type == FE_FONTSMOOTHINGCLEARTYPE;
}
#endif

struct FontDefault {
  const char* pref_name;
  int resource_id;
};

// Font pref defaults.  The prefs that have defaults vary by platform, since not
// all platforms have fonts for all scripts for all generic families.
// TODO(falken): add proper defaults when possible for all
// platforms/scripts/generic families.
const FontDefault kFontDefaults[] = {
    {prefs::kWebKitStandardFontFamily, IDS_STANDARD_FONT_FAMILY},
    {prefs::kWebKitFixedFontFamily, IDS_FIXED_FONT_FAMILY},
    {prefs::kWebKitSerifFontFamily, IDS_SERIF_FONT_FAMILY},
    {prefs::kWebKitSansSerifFontFamily, IDS_SANS_SERIF_FONT_FAMILY},
    {prefs::kWebKitCursiveFontFamily, IDS_CURSIVE_FONT_FAMILY},
    {prefs::kWebKitFantasyFontFamily, IDS_FANTASY_FONT_FAMILY},
    {prefs::kWebKitMathFontFamily, IDS_MATH_FONT_FAMILY},
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {prefs::kWebKitStandardFontFamilyJapanese,
     IDS_STANDARD_FONT_FAMILY_JAPANESE},
    {prefs::kWebKitFixedFontFamilyJapanese, IDS_FIXED_FONT_FAMILY_JAPANESE},
    {prefs::kWebKitSerifFontFamilyJapanese, IDS_SERIF_FONT_FAMILY_JAPANESE},
    {prefs::kWebKitSansSerifFontFamilyJapanese,
     IDS_SANS_SERIF_FONT_FAMILY_JAPANESE},
    {prefs::kWebKitStandardFontFamilyKorean, IDS_STANDARD_FONT_FAMILY_KOREAN},
    {prefs::kWebKitSerifFontFamilyKorean, IDS_SERIF_FONT_FAMILY_KOREAN},
    {prefs::kWebKitSansSerifFontFamilyKorean,
     IDS_SANS_SERIF_FONT_FAMILY_KOREAN},
    {prefs::kWebKitStandardFontFamilySimplifiedHan,
     IDS_STANDARD_FONT_FAMILY_SIMPLIFIED_HAN},
    {prefs::kWebKitSerifFontFamilySimplifiedHan,
     IDS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN},
    {prefs::kWebKitSansSerifFontFamilySimplifiedHan,
     IDS_SANS_SERIF_FONT_FAMILY_SIMPLIFIED_HAN},
    {prefs::kWebKitStandardFontFamilyTraditionalHan,
     IDS_STANDARD_FONT_FAMILY_TRADITIONAL_HAN},
    {prefs::kWebKitSerifFontFamilyTraditionalHan,
     IDS_SERIF_FONT_FAMILY_TRADITIONAL_HAN},
    {prefs::kWebKitSansSerifFontFamilyTraditionalHan,
     IDS_SANS_SERIF_FONT_FAMILY_TRADITIONAL_HAN},
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {prefs::kWebKitCursiveFontFamilySimplifiedHan,
     IDS_CURSIVE_FONT_FAMILY_SIMPLIFIED_HAN},
    {prefs::kWebKitCursiveFontFamilyTraditionalHan,
     IDS_CURSIVE_FONT_FAMILY_TRADITIONAL_HAN},
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {prefs::kWebKitStandardFontFamilyArabic, IDS_STANDARD_FONT_FAMILY_ARABIC},
    {prefs::kWebKitSerifFontFamilyArabic, IDS_SERIF_FONT_FAMILY_ARABIC},
    {prefs::kWebKitSansSerifFontFamilyArabic,
     IDS_SANS_SERIF_FONT_FAMILY_ARABIC},
    {prefs::kWebKitFixedFontFamilyKorean, IDS_FIXED_FONT_FAMILY_KOREAN},
    {prefs::kWebKitFixedFontFamilySimplifiedHan,
     IDS_FIXED_FONT_FAMILY_SIMPLIFIED_HAN},
    {prefs::kWebKitFixedFontFamilyTraditionalHan,
     IDS_FIXED_FONT_FAMILY_TRADITIONAL_HAN},
#elif BUILDFLAG(IS_WIN)
    {prefs::kWebKitFixedFontFamilyArabic, IDS_FIXED_FONT_FAMILY_ARABIC},
    {prefs::kWebKitSansSerifFontFamilyArabic,
     IDS_SANS_SERIF_FONT_FAMILY_ARABIC},
    {prefs::kWebKitStandardFontFamilyCyrillic,
     IDS_STANDARD_FONT_FAMILY_CYRILLIC},
    {prefs::kWebKitFixedFontFamilyCyrillic, IDS_FIXED_FONT_FAMILY_CYRILLIC},
    {prefs::kWebKitSerifFontFamilyCyrillic, IDS_SERIF_FONT_FAMILY_CYRILLIC},
    {prefs::kWebKitSansSerifFontFamilyCyrillic,
     IDS_SANS_SERIF_FONT_FAMILY_CYRILLIC},
    {prefs::kWebKitStandardFontFamilyGreek, IDS_STANDARD_FONT_FAMILY_GREEK},
    {prefs::kWebKitFixedFontFamilyGreek, IDS_FIXED_FONT_FAMILY_GREEK},
    {prefs::kWebKitSerifFontFamilyGreek, IDS_SERIF_FONT_FAMILY_GREEK},
    {prefs::kWebKitSansSerifFontFamilyGreek, IDS_SANS_SERIF_FONT_FAMILY_GREEK},
    {prefs::kWebKitFixedFontFamilyKorean, IDS_FIXED_FONT_FAMILY_KOREAN},
    {prefs::kWebKitCursiveFontFamilyKorean, IDS_CURSIVE_FONT_FAMILY_KOREAN},
    {prefs::kWebKitFixedFontFamilySimplifiedHan,
     IDS_FIXED_FONT_FAMILY_SIMPLIFIED_HAN},
    {prefs::kWebKitFixedFontFamilyTraditionalHan,
     IDS_FIXED_FONT_FAMILY_TRADITIONAL_HAN},
#endif
};

const size_t kFontDefaultsLength = std::size(kFontDefaults);

// Returns the script of the font pref |pref_name|.  For example, suppose
// |pref_name| is "webkit.webprefs.fonts.serif.Hant".  Since the script code for
// the script name "Hant" is USCRIPT_TRADITIONAL_HAN, the function returns
// USCRIPT_TRADITIONAL_HAN.  |pref_name| must be a valid font pref name.
UScriptCode GetScriptOfFontPref(const char* pref_name) {
  // ICU script names are four letters.
  static const size_t kScriptNameLength = 4;

  size_t len = strlen(pref_name);
  DCHECK_GT(len, kScriptNameLength);
  const char* scriptName = &pref_name[len - kScriptNameLength];
  int32_t code = u_getPropertyValueEnum(UCHAR_SCRIPT, scriptName);
  DCHECK(code >= 0 && code < USCRIPT_CODE_LIMIT);
  return static_cast<UScriptCode>(code);
}

// Returns the primary script used by the browser's UI locale.  For example, if
// the locale is "ru", the function returns USCRIPT_CYRILLIC, and if the locale
// is "en", the function returns USCRIPT_LATIN.
UScriptCode GetScriptOfBrowserLocale(const std::string& locale) {
  // For Chinese locales, uscript_getCode() just returns USCRIPT_HAN but our
  // per-script fonts are for USCRIPT_SIMPLIFIED_HAN and
  // USCRIPT_TRADITIONAL_HAN.
  if (locale == "zh-CN")
    return USCRIPT_SIMPLIFIED_HAN;
  if (locale == "zh-TW")
    return USCRIPT_TRADITIONAL_HAN;
  // For Korean and Japanese, multiple scripts are returned by
  // |uscript_getCode|, but we're passing a one entry buffer leading
  // the buffer to be filled by USCRIPT_INVALID_CODE. We need to
  // hard-code the results for them.
  if (locale == "ko")
    return USCRIPT_HANGUL;
  if (locale == "ja")
    return USCRIPT_JAPANESE;

  UScriptCode code = USCRIPT_INVALID_CODE;
  UErrorCode err = U_ZERO_ERROR;
  uscript_getCode(locale.c_str(), &code, 1, &err);

  if (U_FAILURE(err))
    code = USCRIPT_INVALID_CODE;
  return code;
}

// Sets a font family pref in |prefs| to |pref_value|.
void OverrideFontFamily(blink::web_pref::WebPreferences* prefs,
                        const std::string& generic_family,
                        const std::string& script,
                        const std::string& pref_value) {
  blink::web_pref::ScriptFontFamilyMap* map = nullptr;
  if (generic_family == "standard")
    map = &prefs->standard_font_family_map;
  else if (generic_family == "fixed")
    map = &prefs->fixed_font_family_map;
  else if (generic_family == "serif")
    map = &prefs->serif_font_family_map;
  else if (generic_family == "sansserif")
    map = &prefs->sans_serif_font_family_map;
  else if (generic_family == "cursive")
    map = &prefs->cursive_font_family_map;
  else if (generic_family == "fantasy")
    map = &prefs->fantasy_font_family_map;
  else if (generic_family == "math")
    map = &prefs->math_font_family_map;
  else
    NOTREACHED_IN_MIGRATION()
        << "Unknown generic font family: " << generic_family;
  (*map)[script] = base::UTF8ToUTF16(pref_value);
}

#if !BUILDFLAG(IS_ANDROID)
void RegisterLocalizedFontPref(user_prefs::PrefRegistrySyncable* registry,
                               const char* path,
                               int default_message_id) {
  int val = 0;
  bool success = base::StringToInt(l10n_util::GetStringUTF8(
      default_message_id), &val);
  DCHECK(success);
  registry->RegisterIntegerPref(path, val);
}
#endif

}  // namespace

PrefsTabHelper::PrefsTabHelper(WebContents* contents)
    : content::WebContentsUserData<PrefsTabHelper>(*contents),
      profile_(Profile::FromBrowserContext(contents->GetBrowserContext())) {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs) {
#if !BUILDFLAG(IS_ANDROID)
    // If the tab is in an incognito profile, we track changes in the default
    // zoom level of the parent profile instead.
    Profile* profile_to_track = profile_->GetOriginalProfile();
    ChromeZoomLevelPrefs* zoom_level_prefs =
        profile_to_track->GetZoomLevelPrefs();

    // Tests should not need to create a ZoomLevelPrefs.
    if (zoom_level_prefs) {
      default_zoom_level_subscription_ =
          zoom_level_prefs->RegisterDefaultZoomLevelCallback(
              base::BindRepeating(&PrefsTabHelper::UpdateRendererPreferences,
                                  base::Unretained(this)));
    }

    // Unretained is safe because the registrar will be scoped to this class.
    font_change_registrar_.Register(
        FontPrefChangeNotifierFactory::GetForProfile(profile_),
        base::BindRepeating(&PrefsTabHelper::OnWebPrefChanged,
                            base::Unretained(this)));
#endif  // !BUILDFLAG(IS_ANDROID)

    PrefWatcher::Get(profile_)->RegisterHelper(this);
  }

  blink::RendererPreferences* render_prefs =
      GetWebContents().GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(render_prefs, profile_);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
  ThemeServiceFactory::GetForProfile(profile_)->AddObserver(this);
#endif
}

PrefsTabHelper::~PrefsTabHelper() {
  PrefWatcher::Get(profile_)->UnregisterHelper(this);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
  ThemeServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
#endif
}

// static
void PrefsTabHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry,
    const std::string& locale) {
  WebPreferences pref_defaults;
  registry->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                                pref_defaults.javascript_enabled);
  registry->RegisterBooleanPref(prefs::kWebKitWebSecurityEnabled,
                                pref_defaults.web_security_enabled);
  registry->RegisterBooleanPref(prefs::kWebKitLoadsImagesAutomatically,
                                pref_defaults.loads_images_automatically);
  registry->RegisterBooleanPref(prefs::kWebKitPluginsEnabled,
                                pref_defaults.plugins_enabled);
  registry->RegisterBooleanPref(prefs::kWebKitDomPasteEnabled,
                                pref_defaults.dom_paste_enabled);
  registry->RegisterBooleanPref(prefs::kWebKitTextAreasAreResizable,
                                pref_defaults.text_areas_are_resizable);
  registry->RegisterBooleanPref(prefs::kWebKitJavascriptCanAccessClipboard,
                                pref_defaults.javascript_can_access_clipboard);
  registry->RegisterBooleanPref(prefs::kWebkitTabsToLinks,
                                pref_defaults.tabs_to_links);
  registry->RegisterBooleanPref(prefs::kWebKitAllowRunningInsecureContent,
                                false);
  registry->RegisterBooleanPref(
      prefs::kEnableReferrers,
      !base::FeatureList::IsEnabled(features::kNoReferrers));
  registry->RegisterBooleanPref(prefs::kEnableEncryptedMedia, true);
  registry->RegisterStringPref(prefs::kPrefixedVideoFullscreenApiAvailability,
                               "runtime-enabled");
  registry->RegisterBooleanPref(prefs::kScrollToTextFragmentEnabled, true);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterDoublePref(browser_ui::prefs::kWebKitFontScaleFactor, 1.0);
  registry->RegisterIntegerPref(prefs::kAccessibilityTextSizeContrastFactor, 0);
  registry->RegisterBooleanPref(browser_ui::prefs::kWebKitForceEnableZoom,
                                pref_defaults.force_enable_zoom);
  registry->RegisterBooleanPref(prefs::kWebKitPasswordEchoEnabled,
                                pref_defaults.password_echo_enabled);
  registry->RegisterIntegerPref(prefs::kAccessibilityFontWeightAdjustment, 0);

#endif

  bool force_dark_mode_enabled =
      base::FeatureList::IsEnabled(blink::features::kForceWebContentsDarkMode)
          ? true
          : pref_defaults.force_dark_mode_enabled;
  registry->RegisterBooleanPref(prefs::kWebKitForceDarkModeEnabled,
                                force_dark_mode_enabled);
  registry->RegisterStringPref(
      prefs::kDefaultCharset,
      l10n_util::GetStringUTF8(IDS_DEFAULT_ENCODING),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Register font prefs that have defaults.
  std::set<std::string> fonts_with_defaults;
  UScriptCode browser_script = GetScriptOfBrowserLocale(locale);
  for (size_t i = 0; i < kFontDefaultsLength; ++i) {
    FontDefault pref = kFontDefaults[i];

#if BUILDFLAG(IS_WIN)
    if (pref.pref_name == prefs::kWebKitFixedFontFamily) {
      if (ShouldUseAlternateDefaultFixedFont(
              l10n_util::GetStringUTF8(pref.resource_id)))
        pref.resource_id = IDS_FIXED_FONT_FAMILY_ALT_WIN;
    }
#endif

    UScriptCode pref_script = GetScriptOfFontPref(pref.pref_name);

    // Suppress this default font pref value if it is for the primary script of
    // the browser's UI locale.  For example, if the pref is for the sans-serif
    // font for the Cyrillic script, and the browser locale is "ru" (Russian),
    // the default is suppressed.  Otherwise, the default would override the
    // user's font preferences when viewing pages in their native language.
    // This is because users have no way yet of customizing their per-script
    // font preferences.  The font prefs accessible in the options UI are for
    // the default, unknown script; these prefs have less priority than the
    // per-script font prefs when the script of the content is known.  This code
    // can possibly be removed later if users can easily access per-script font
    // prefs (e.g., via the extensions workflow), or the problem turns out to
    // not be really critical after all.
    if (browser_script != pref_script) {
      std::string value = l10n_util::GetStringUTF8(pref.resource_id);
#if PREFS_FONT_LIST
      if (value.starts_with(',')) {
        value = gfx::FontList::FirstAvailableOrFirst(value);
      }
#else   // !PREFS_FONT_LIST
      DCHECK(!value.starts_with(','))
          << "This platform doesn't support default font lists. "
          << pref.pref_name << "=" << value;
#endif  // PREFS_FONT_LIST
      registry->RegisterStringPref(pref.pref_name, value);
      fonts_with_defaults.insert(pref.pref_name);
    }
  }

// Register font prefs.  This is only configurable on desktop Chrome.
#if !BUILDFLAG(IS_ANDROID)
  RegisterFontFamilyPrefs(registry, fonts_with_defaults);

  registry->RegisterIntegerPref(prefs::kWebKitDefaultFontSize, 16);
  registry->RegisterIntegerPref(prefs::kWebKitDefaultFixedFontSize, 13);
  registry->RegisterIntegerPref(prefs::kWebKitMinimumFontSize, 0);
  RegisterLocalizedFontPref(registry, prefs::kWebKitMinimumLogicalFontSize,
                            IDS_MINIMUM_LOGICAL_FONT_SIZE);
#endif
}

// static
void PrefsTabHelper::GetServiceInstance() {
  PrefWatcherFactory::GetInstance();
}

void PrefsTabHelper::OnThemeChanged() {
  UpdateRendererPreferences();
}

void PrefsTabHelper::UpdateWebPreferences() {
  GetWebContents().NotifyPreferencesChanged();
}

void PrefsTabHelper::UpdateRendererPreferences() {
  blink::RendererPreferences* prefs =
      GetWebContents().GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile_);
  GetWebContents().SyncRendererPrefs();
}

void PrefsTabHelper::OnFontFamilyPrefChanged(const std::string& pref_name) {
  // When a font family pref's value goes from non-empty to the empty string, we
  // must add it to the usual WebPreferences struct passed to the renderer.
  //
  // The empty string means to fall back to the pref for the Common script
  // ("Zyyy").  For example, if chrome.fonts.serif.Cyrl is the empty string, it
  // means to use chrome.fonts.serif.Zyyy for Cyrillic script. Prefs that are
  // the empty string are normally not passed to WebKit, since there are so many
  // of them that it would cause a performance regression. Not passing the pref
  // is normally okay since WebKit does the desired fallback behavior regardless
  // of whether the empty string is passed or the pref is not passed at all. But
  // if the pref has changed from non-empty to the empty string, we must let
  // WebKit know.
  std::string generic_family;
  std::string script;
  if (pref_names_util::ParseFontNamePrefPath(pref_name,
                                             &generic_family,
                                             &script)) {
    PrefService* prefs = profile_->GetPrefs();
    std::string pref_value = prefs->GetString(pref_name);
    if (pref_value.empty()) {
      blink::web_pref::WebPreferences web_prefs =
          GetWebContents().GetOrCreateWebPreferences();
      OverrideFontFamily(&web_prefs, generic_family, script, std::string());
      GetWebContents().SetWebPreferences(web_prefs);
      return;
    }
  }
}

void PrefsTabHelper::OnWebPrefChanged(const std::string& pref_name) {
  // Use PostTask to dispatch the OnWebkitPreferencesChanged notification to
  // give other observers (particularly the FontFamilyCache) a chance to react
  // to the pref change.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrefsTabHelper::NotifyWebkitPreferencesChanged,
                                weak_ptr_factory_.GetWeakPtr(), pref_name));
}

void PrefsTabHelper::NotifyWebkitPreferencesChanged(
    const std::string& pref_name) {
#if !BUILDFLAG(IS_ANDROID)
  OnFontFamilyPrefChanged(pref_name);
#endif

  GetWebContents().OnWebPreferencesChanged();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrefsTabHelper);
