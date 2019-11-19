// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace language {
class LanguagePrefs;
}

namespace translate {

// Enables or disables using the most recent target language as the default
// target language option.
extern const base::Feature kTranslateRecentTarget;

// Enable or disable the Translate popup altogether.
extern const base::Feature kTranslateUI;

// Minimum number of times the user must accept a translation before we show
// a shortcut to the "Always Translate" functionality.
#if defined(OS_ANDROID) || defined(OS_IOS)
// The "Always Translate" shortcut is always shown on iOS and Android.
constexpr int kAlwaysTranslateShortcutMinimumAccepts = 1;
#else
constexpr int kAlwaysTranslateShortcutMinimumAccepts = 3;
#endif

// Minimum number of times the user must deny a translation before we show
// a shortcut to the "Never Translate" functionality.
// Android and iOS implementations do not offer a drop down (for space reasons),
// so we are more aggressive about showing this shortcut.
#if defined(OS_ANDROID)
// On Android, this shows the "Never Translate" shortcut after two denials just
// like on iOS. However, the last event is not counted so we must subtract one
// to get the same behavior.
constexpr int kNeverTranslateShortcutMinimumDenials = 1;
#elif defined(OS_IOS)
constexpr int kNeverTranslateShortcutMinimumDenials = 2;
#else
constexpr int kNeverTranslateShortcutMinimumDenials = 3;
#endif

class TranslateAcceptLanguages;

// Allows updating denial times for a specific language while maintaining the
// maximum list size and ensuring PrefObservers are notified of change values.
class DenialTimeUpdate {
 public:
  DenialTimeUpdate(PrefService* prefs,
                   const std::string& language,
                   size_t max_denial_count);
  ~DenialTimeUpdate();

  // Gets the list of timestamps when translation was denied. Guaranteed to
  // be non-null, potentially inserts a new listvalue into the dictionary if
  // necessary.
  base::ListValue* GetDenialTimes();

  // Gets the oldest denial time on record. Will return base::Time::max() if
  // no denial time has been recorded yet.
  base::Time GetOldestDenialTime();

  // Records a new denial time. Does not ensure ordering of denial times - it is
  // up to the user to ensure times are in monotonic order.
  void AddDenialTime(base::Time denial_time);

 private:
  DictionaryPrefUpdate denial_time_dict_update_;
  std::string language_;
  size_t max_denial_count_;
  base::ListValue* time_list_;  // Weak, owned by the containing prefs service.
};

// This class holds various info about a language, that are related to Translate
// Preferences and Language Settings.
struct TranslateLanguageInfo {
  TranslateLanguageInfo();
  TranslateLanguageInfo(const TranslateLanguageInfo&);

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
  // TODO(crbug.com/524927): Remove kPrefTranslateSiteBlacklist after
  // 3 milestones (M74).
  static const char kPrefTranslateSiteBlacklistDeprecated[];
  static const char kPrefTranslateSiteBlacklistWithTime[];
  static const char kPrefTranslateWhitelists[];
  static const char kPrefTranslateDeniedCount[];
  static const char kPrefTranslateIgnoredCount[];
  static const char kPrefTranslateAcceptedCount[];
  static const char kPrefTranslateLastDeniedTimeForLanguage[];
  static const char kPrefTranslateTooOftenDeniedForLanguage[];
  static const char kPrefTranslateRecentTarget[];
#if defined(OS_ANDROID) || defined(OS_IOS)
  static const char kPrefTranslateAutoAlwaysCount[];
  static const char kPrefTranslateAutoNeverCount[];
#endif
#if defined(OS_ANDROID)
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

  // |preferred_languages_pref| is only used on Chrome OS, other platforms must
  // pass NULL.
  TranslatePrefs(PrefService* user_prefs,
                 const char* accept_languages_pref,
                 const char* preferred_languages_pref);

  ~TranslatePrefs();

  // Checks if the "offer translate" (i.e. automatic translate bubble) feature
  // is enabled.
  bool IsOfferTranslateEnabled() const;

  // Checks if translate is allowed by policy.
  bool IsTranslateAllowedByPolicy() const;

  // Sets the country that the application is run in. Determined by the
  // VariationsService, can be left empty. Used by the TranslateRanker.
  void SetCountry(const std::string& country);
  std::string GetCountry() const;

  // Resets the blocked languages list, the sites blacklist, the languages
  // whitelist, and the accepted/denied counts.
  void ResetToDefaults();

  bool IsBlockedLanguage(const std::string& original_language) const;
  void BlockLanguage(const std::string& original_language);
  void UnblockLanguage(const std::string& original_language);

  // Adds the language to the language list at chrome://settings/languages.
  // If the param |force_blocked| is set to true, the language is added to the
  // blocked list.
  // If force_blocked is set to false, the language is added to the blocked list
  // if the language list does not already contain another language with the
  // same base language.
  void AddToLanguageList(const std::string& language, bool force_blocked);
  // Removes the language from the language list at chrome://settings/languages.
  void RemoveFromLanguageList(const std::string& language);

  // Rearranges the given language inside the language list.
  // The direction of the move is specified as a RearrangeSpecifier.
  // |offset| is ignored unless the RearrangeSpecifier is kUp or kDown: in
  // which case it needs to be positive for any change to be made.
  // The param |enabled_languages| is a list of languages that are enabled in
  // the current UI. This is required because the full language list contains
  // some languages that might not be enabled in the current UI and we need to
  // skip those languages while rearranging the list.
  void RearrangeLanguage(const std::string& language,
                         RearrangeSpecifier where,
                         const int offset,
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

  bool IsSiteBlacklisted(const std::string& site) const;
  void BlacklistSite(const std::string& site);
  void RemoveSiteFromBlacklist(const std::string& site);

  std::vector<std::string> GetBlacklistedSitesBetween(base::Time begin,
                                                      base::Time end) const;
  void DeleteBlacklistedSitesBetween(base::Time begin, base::Time end);

  bool HasWhitelistedLanguagePairs() const;

  bool IsLanguagePairWhitelisted(const std::string& original_language,
                                 const std::string& target_language);
  void WhitelistLanguagePair(const std::string& original_language,
                             const std::string& target_language);
  void RemoveLanguagePairFromWhitelist(const std::string& original_language,
                                       const std::string& target_language);

  // Will return true if at least one site has been blacklisted.
  bool HasBlacklistedSites() const;

  // These methods are used to track how many times the user has denied the
  // translation for a specific language. (So we can present a UI to black-list
  // that language if the user keeps denying translations).
  int GetTranslationDeniedCount(const std::string& language) const;
  void IncrementTranslationDeniedCount(const std::string& language);
  void ResetTranslationDeniedCount(const std::string& language);

  // These methods are used to track how many times the user has ignored the
  // translation bubble for a specific language.
  int GetTranslationIgnoredCount(const std::string& language) const;
  void IncrementTranslationIgnoredCount(const std::string& language);
  void ResetTranslationIgnoredCount(const std::string& language);

  // These methods are used to track how many times the user has accepted the
  // translation for a specific language. (So we can present a UI to white-list
  // that language if the user keeps accepting translations).
  int GetTranslationAcceptedCount(const std::string& language) const;
  void IncrementTranslationAcceptedCount(const std::string& language);
  void ResetTranslationAcceptedCount(const std::string& language);

#if defined(OS_ANDROID) || defined(OS_IOS)
  // These methods are used to track how many times the auto-always translation
  // has been triggered for a specific language.
  int GetTranslationAutoAlwaysCount(const std::string& language) const;
  void IncrementTranslationAutoAlwaysCount(const std::string& language);
  void ResetTranslationAutoAlwaysCount(const std::string& language);

  // These methods are used to track how many times the auto-never translation
  // has been triggered for a specific language.
  int GetTranslationAutoNeverCount(const std::string& language) const;
  void IncrementTranslationAutoNeverCount(const std::string& language);
  void ResetTranslationAutoNeverCount(const std::string& language);
#endif

#if defined(OS_ANDROID)
  // These methods are used to determine whether the explicit language ask
  // prompt was displayed to the user already.
  bool GetExplicitLanguageAskPromptShown() const;
  void SetExplicitLanguageAskPromptShown(bool shown);
#endif

  // Update the last time on closing the Translate UI without translation.
  void UpdateLastDeniedTime(const std::string& language);

  // Returns true if translation is denied too often.
  bool IsTooOftenDenied(const std::string& language) const;

  // Resets the prefs of denial state. Only used internally for diagnostics.
  void ResetDenialState();

  // Gets the language list of the language settings.
  void GetLanguageList(std::vector<std::string>* languages) const;

  bool CanTranslateLanguage(TranslateAcceptLanguages* accept_languages,
                            const std::string& language);
  bool ShouldAutoTranslate(const std::string& original_language,
                           std::string* target_language);

  // Stores and retrieves the last-observed translate target language. Used to
  // determine which target language to offer in future.
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

  // Migrate the sites blacklist from a list to a dictionary that maps sites
  // to a timestamp of the creation of this entry.
  void MigrateSitesBlacklist();

  // Prevent empty blocked languages by resetting them to the default value.
  // (crbug.com/902354)
  void ResetEmptyBlockedLanguagesToDefaults();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, UpdateLanguageList);
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
  friend class TranslatePrefsTest;

  // Updates the language list of the language settings.
  void UpdateLanguageList(const std::vector<std::string>& languages);

  void ResetBlockedLanguagesToDefault();
  void ClearBlacklistedSites();
  void ClearWhitelistedLanguagePairs();

  // |pref_id| is the name of a list pref.
  bool IsValueBlacklisted(const char* pref_id, const std::string& value) const;
  void BlacklistValue(const char* pref_id, const std::string& value);
  void RemoveValueFromBlacklist(const char* pref_id, const std::string& value);
  bool IsValueInList(const base::ListValue* list,
                     const std::string& value) const;
  size_t GetListSize(const char* pref_id) const;

  bool IsDictionaryEmpty(const char* pref_id) const;
  // Removes from the language list any language that isn't supported as an
  // Accept-Language (it's not in kAcceptLanguageList) if and only if there
  // aren't any other languages from the same family in the list that are
  // supported.
  void PurgeUnsupportedLanguagesInLanguageFamily(
      const std::string& language,
      std::vector<std::string>* list);

  // Path to the preference storing the accept languages.
  const std::string accept_languages_pref_;
#if defined(OS_CHROMEOS)
  // Path to the preference storing the preferred languages.
  // Only used on ChromeOS.
  std::string preferred_languages_pref_;
#endif

  // Retrieves the dictionary mapping the number of times translation has been
  // denied for a language, creating it if necessary.
  base::DictionaryValue* GetTranslationDeniedCountDictionary();

  // Retrieves the dictionary mapping the number of times translation has been
  // accepted for a language, creating it if necessary.
  base::DictionaryValue* GetTranslationAcceptedCountDictionary() const;

  // Returns the languages that should be blocked by default as a
  // base::(List)Value.
  static base::Value GetDefaultBlockedLanguages();

  PrefService* prefs_;  // Weak.

  std::string country_;  // The country the app runs in.

  std::unique_ptr<language::LanguagePrefs> language_prefs_;

  DISALLOW_COPY_AND_ASSIGN(TranslatePrefs);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREFS_H_
