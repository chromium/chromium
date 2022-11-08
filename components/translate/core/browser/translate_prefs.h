// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREFS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREFS_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class DictionaryValue;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace language {
class LanguagePrefs;
}

namespace translate {

// Enables or disables using the most recent target language as the default
// target language option.
BASE_DECLARE_FEATURE(kTranslateRecentTarget);

// This allows the user to disable translate by using the
// `--disable-features=Translate` command-line flag.
BASE_DECLARE_FEATURE(kTranslate);

// Whether to migrate the obsolete always-translate languages pref to the new
// pref during object construction as a fix for crbug/1291356, which had
// previously not been migrated at all on iOS. This also enables a more
// conservative pref merging process that aims to merge in old always-translate
// language values from the obsolete pref without conflicting with any values in
// the new pref that may have been added.
//
// TODO(crbug/1303963): This base::Feature only exists to allow a less risky
// merge into iOS M98. This base::Feature should be removed once it's no longer
// relevant and the enabled behavior should become the only behavior.
BASE_DECLARE_FEATURE(kMigrateAlwaysTranslateLanguagesFix);

// Minimum number of times the user must accept a translation before we show
// a shortcut to the "Always Translate" functionality.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// The "Always Translate" shortcut is always shown on iOS and Android.
constexpr int kAlwaysTranslateShortcutMinimumAccepts = 1;
#else
constexpr int kAlwaysTranslateShortcutMinimumAccepts = 3;
#endif

// Minimum number of times the user must deny a translation before we show
// a shortcut to the "Never Translate" functionality.
// Android and iOS implementations do not offer a drop down (for space reasons),
// so we are more aggressive about showing this shortcut.
#if BUILDFLAG(IS_ANDROID)
// On Android, this shows the "Never Translate" shortcut after two denials just
// like on iOS. However, the last event is not counted so we must subtract one
// to get the same behavior.
constexpr int kNeverTranslateShortcutMinimumDenials = 1;
#elif BUILDFLAG(IS_IOS)
constexpr int kNeverTranslateShortcutMinimumDenials = 2;
#else
constexpr int kNeverTranslateShortcutMinimumDenials = 3;
#endif

// This class holds various info about a language, that are related to Translate
// Preferences and Language Settings.
struct TranslateLanguageInfo {
  TranslateLanguageInfo();

  TranslateLanguageInfo(const TranslateLanguageInfo&);
  TranslateLanguageInfo(TranslateLanguageInfo&&) noexcept;
  TranslateLanguageInfo& operator=(const TranslateLanguageInfo&);
  TranslateLanguageInfo& operator=(TranslateLanguageInfo&&) noexcept;

  // This ISO code of the language.
  std::string code;
  // The display name of the language in the current locale.
  std::string display_name;
  // The display name of the language in the language locale.
  std::string native_display_name;
  // Whether we support translate for this language.
  bool supports_translate = false;
};

// The wrapper of PrefService object for Translate.
//
// It is assumed that |prefs_| is alive while this instance is alive.
class TranslatePrefs {
 public:
  static const char kPrefLanguageProfile[];
  static const char kPrefForceTriggerTranslateCount[];
  // TODO(crbug.com/524927): Remove kPrefNeverPromptSites after
  // 3 milestones (M74).
  static const char kPrefNeverPromptSitesDeprecated[];
  static const char kPrefTranslateDeniedCount[];
  static const char kPrefTranslateIgnoredCount[];
  static const char kPrefTranslateAcceptedCount[];
  // Deprecated 10/2021.
  static const char kPrefAlwaysTranslateListDeprecated[];
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  static const char kPrefTranslateAutoAlwaysCount[];
  static const char kPrefTranslateAutoNeverCount[];
#endif
#if BUILDFLAG(IS_ANDROID)
  static const char kPrefExplicitLanguageAskShown[];
#endif

  // This parameter specifies how the language should be moved within the list.
  enum RearrangeSpecifier {
    // No-op enumerator.
    kNone,
    // Move the language to the very top of the list.
    kTop,
    // Move the language up towards the front of the list.
    kUp,
    // Move the language down towards the back of the list.
    kDown
  };

  explicit TranslatePrefs(PrefService* user_prefs);

  TranslatePrefs(const TranslatePrefs&) = delete;
  TranslatePrefs& operator=(const TranslatePrefs&) = delete;

  ~TranslatePrefs();

  // Some existing preferences do not follow inclusive naming. Existing
  // preference names cannot be renamed since values are saved client side.
  // Map these to inclusive alternatives to reduce references to those names in
  // the rest of the code.
  static std::string MapPreferenceName(const std::string& pref_name);

  // Returns true if the "offer translate" pref is enabled (i.e. allowing for
  // automatic Full Page Translate bubbles).
  bool IsOfferTranslateEnabled() const;

  // Returns true if Translate is allowed by policy.
  bool IsTranslateAllowedByPolicy() const;

  // Sets the country that the application is run in. Determined by the
  // VariationsService, can be left empty. Used by the TranslateRanker.
  void SetCountry(const std::string& country);
  std::string GetCountry() const;

  // Resets the blocked languages list, the never-translate site list, the
  // always-translate languages list, the accepted/denied counts, and whether
  // Translate is enabled.
  void ResetToDefaults();

  // Before adding to, removing from, or checking the block list the source
  // language is converted to its translate synonym.
  // A blocked language will not be offered to be translated. All blocked
  // languages form the "Never translate" list.
  bool IsBlockedLanguage(base::StringPiece source_language) const;
  void BlockLanguage(base::StringPiece source_language);
  void UnblockLanguage(base::StringPiece source_language);
  // Returns the languages that should be blocked by default as a
  // base::Value::List.
  static base::Value::List GetDefaultBlockedLanguages();
  void ResetBlockedLanguagesToDefault();
  // Prevent empty blocked languages by resetting them to the default value.
  // (crbug.com/902354)
  void ResetEmptyBlockedLanguagesToDefaults();
  // Get the languages that for which translation should never be prompted
  // formatted as Chrome language codes.
  std::vector<std::string> GetNeverTranslateLanguages() const;

  // Adds the language to the language list at chrome://settings/languages.
  // If the param |force_blocked| is set to true, the language is added to the
  // blocked list.
  // If force_blocked is set to false, the language is added to the blocked list
  // if the language list does not already contain another language with the
  // same base language.
  void AddToLanguageList(base::StringPiece language, bool force_blocked);
  // Removes the language from the language list at chrome://settings/languages.
  void RemoveFromLanguageList(base::StringPiece language);

  // Rearranges the given language inside the language list.
  // The direction of the move is specified as a RearrangeSpecifier.
  // |offset| is ignored unless the RearrangeSpecifier is kUp or kDown: in
  // which case it needs to be positive for any change to be made.
  // The param |enabled_languages| is a list of languages that are enabled in
  // the current UI. This is required because the full language list contains
  // some languages that might not be enabled in the current UI and we need to
  // skip those languages while rearranging the list.
  void RearrangeLanguage(base::StringPiece language,
                         RearrangeSpecifier where,
                         int offset,
                         const std::vector<std::string>& enabled_languages);

  // Sets the language order to the provided order.
  // This function is called from the language preference manager in Chrome for
  // Android.
  void SetLanguageOrder(const std::vector<std::string>& new_order);

  // Returns the list of TranslateLanguageInfo for all languages that are
  // available in the given locale.
  // The list returned in |languages| is sorted alphabetically based on the
  // display names in the given locale.
  // May cause a supported language list fetch unless |translate_allowed| is
  // false.
  static void GetLanguageInfoList(
      const std::string& app_locale,
      bool translate_allowed,
      std::vector<TranslateLanguageInfo>* languages);

  // Returns a list of language codes representing content language set by the
  // user that are translatable for given app_language. The list returned in
  // |codes| is ordered based on the user's ordering. In case user has
  // country variants for a specific language set, the language main
  // translatable language is returned, e.g. if a user has "de" and "de-CH", the
  // result is "de", if a user only has "de-CH" content language set, "de" is
  // returned.
  void GetTranslatableContentLanguages(const std::string& app_locale,
                                       std::vector<std::string>* codes);

  bool IsSiteOnNeverPromptList(base::StringPiece site) const;
  void AddSiteToNeverPromptList(base::StringPiece site);
  void RemoveSiteFromNeverPromptList(base::StringPiece site);

  std::vector<std::string> GetNeverPromptSitesBetween(base::Time begin,
                                                      base::Time end) const;
  void DeleteNeverPromptSitesBetween(base::Time begin, base::Time end);

  bool HasLanguagePairsToAlwaysTranslate() const;

  bool IsLanguagePairOnAlwaysTranslateList(base::StringPiece source_language,
                                           base::StringPiece target_language);
  // Converts the source and target language to their translate synonym and
  // adds the pair to the always translate dict.
  void AddLanguagePairToAlwaysTranslateList(base::StringPiece source_language,
                                            base::StringPiece target_language);
  // Removes the translate synonym of source_language from the always
  // translate dict.
  void RemoveLanguagePairFromAlwaysTranslateList(
      base::StringPiece source_language,
      base::StringPiece target_language);

  // Sets the always translate state for a language.
  // The always translate language list is actually a dict mapping
  // source_language -> target_language.  We use the current target language
  // when adding |language| to the dict.
  void SetLanguageAlwaysTranslateState(base::StringPiece source_language,
                                       bool always_translate);

  // Gets the languages that are set to always translate formatted as Chrome
  // language codes.
  std::vector<std::string> GetAlwaysTranslateLanguages() const;

  // These methods are used to track how many times the user has denied the
  // translation for a specific language. (So we can present a UI to blocklist
  // that language if the user keeps denying translations).
  int GetTranslationDeniedCount(base::StringPiece language) const;
  void IncrementTranslationDeniedCount(base::StringPiece language);
  void ResetTranslationDeniedCount(base::StringPiece language);

  // These methods are used to track how many times the user has ignored the
  // translation bubble for a specific language.
  int GetTranslationIgnoredCount(base::StringPiece language) const;
  void IncrementTranslationIgnoredCount(base::StringPiece language);
  void ResetTranslationIgnoredCount(base::StringPiece language);

  // These methods are used to track how many times the user has accepted the
  // translation for a specific language. (So we can present a UI to allowlist
  // that language if the user keeps accepting translations).
  int GetTranslationAcceptedCount(base::StringPiece language) const;
  void IncrementTranslationAcceptedCount(base::StringPiece language);
  void ResetTranslationAcceptedCount(base::StringPiece language);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // These methods are used to track how many times the auto-always translation
  // has been triggered for a specific language.
  int GetTranslationAutoAlwaysCount(base::StringPiece language) const;
  void IncrementTranslationAutoAlwaysCount(base::StringPiece language);
  void ResetTranslationAutoAlwaysCount(base::StringPiece language);

  // These methods are used to track how many times the auto-never translation
  // has been triggered for a specific language.
  int GetTranslationAutoNeverCount(base::StringPiece language) const;
  void IncrementTranslationAutoNeverCount(base::StringPiece language);
  void ResetTranslationAutoNeverCount(base::StringPiece language);
#endif

#if BUILDFLAG(IS_ANDROID)
  // These methods are used to determine whether the explicit language ask
  // prompt was displayed to the user already.
  bool GetExplicitLanguageAskPromptShown() const;
  void SetExplicitLanguageAskPromptShown(bool shown);

  // These methods are used to determine whether the app language prompt was
  // displayed to the user already. Once shown it can not be unset.
  bool GetAppLanguagePromptShown() const;
  void SetAppLanguagePromptShown();
#endif

  // Gets the full (policy-forced and user selected) language list from language
  // settings.
  void GetLanguageList(std::vector<std::string>* languages) const;

  // Gets the user selected language list from language settings.
  void GetUserSelectedLanguageList(std::vector<std::string>* languages) const;

  bool CanTranslateLanguage(base::StringPiece language);
  bool ShouldAutoTranslate(base::StringPiece source_language,
                           std::string* target_language);
  // True if the detailed language settings are enabled for this user.
  static bool IsDetailedLanguageSettingsEnabled();

  // Stores and retrieves the last-observed translate target language. Used to
  // determine which target language to offer in future. The translate target
  // is converted to a translate synonym before it is set.
  void SetRecentTargetLanguage(const std::string& target_language);
  void ResetRecentTargetLanguage();
  std::string GetRecentTargetLanguage() const;

  // Gets the value for the pref that represents how often the
  // kOverrideTranslateTriggerInIndia experiment made translate trigger on an
  // English page when it otherwise wouldn't have. This pref is used to
  // determine whether the experiment should be suppressed for a particular user
  int GetForceTriggerOnEnglishPagesCount() const;
  // Increments the pref that represents how often the
  // kOverrideTranslateTriggerInIndia experiment made translate trigger on an
  // English page when it otherwise wouldn't have.
  void ReportForceTriggerOnEnglishPages();
  // Sets to -1 the pref that represents how often the
  // kOverrideTranslateTriggerInIndia experiment made translate trigger on an
  // English page when it otherwise wouldn't have. This is a special value that
  // signals that the backoff should not happen for that user.
  void ReportAcceptedAfterForceTriggerOnEnglishPages();

  // Migrate the sites to never translate from a list to a dictionary that maps
  // sites to a timestamp of the creation of this entry.
  void MigrateNeverPromptSites();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void RegisterProfilePrefsForMigration(
      user_prefs::PrefRegistrySyncable* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest,
                           UpdateLanguageListFeatureEnabled);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, BlockLanguage);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, UnblockLanguage);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, AddToLanguageList);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, RemoveFromLanguageList);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest,
                           RemoveFromLanguageListRemovesRemainingUnsupported);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest,
                           RemoveFromLanguageListClearsRecentLanguage);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, AddToLanguageList);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, RemoveFromLanguageList);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, MoveLanguageToTheTop);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, MoveLanguageUp);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, MoveLanguageDown);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, ResetBlockedLanguagesToDefault);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, MigrateNeverPromptSites);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, SiteNeverPromptList);
  friend class TranslatePrefsTest;

  void ClearNeverPromptSiteList();
  void ClearAlwaysTranslateLanguagePairs();

  // |pref_id| is the name of a list pref.
  bool IsValueOnNeverPromptList(const char* pref_id,
                                base::StringPiece value) const;
  void AddValueToNeverPromptList(const char* pref_id, base::StringPiece value);
  // Used for testing. The public version passes in base::Time::Now()
  void AddSiteToNeverPromptList(base::StringPiece site, base::Time time);
  void RemoveValueFromNeverPromptList(const char* pref_id,
                                      base::StringPiece value);
  size_t GetListSize(const char* pref_id) const;

  bool IsDictionaryEmpty(const char* pref_id) const;

  // Retrieves the dictionary mapping the number of times translation has been
  // denied for a language, creating it if necessary.
  base::DictionaryValue* GetTranslationDeniedCountDictionary();

  // Retrieves the dictionary mapping the number of times translation has been
  // accepted for a language, creating it if necessary.
  base::DictionaryValue* GetTranslationAcceptedCountDictionary() const;

  raw_ptr<PrefService> prefs_;  // Weak.

  std::string country_;  // The country the app runs in.

  std::unique_ptr<language::LanguagePrefs> language_prefs_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREFS_H_
