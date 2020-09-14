// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_prefs.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

namespace translate {

namespace {

// Returns whether or not the given list includes at least one language with
// the same base as the input language.
// For example: "en-US" and "en-UK" share the same base "en".
bool ContainsSameBaseLanguage(const std::vector<std::string>& list,
                              base::StringPiece language_code) {
  base::StringPiece base_language =
      language::ExtractBaseLanguage(language_code);
  for (const auto& item : list) {
    if (base_language == language::ExtractBaseLanguage(item))
      return true;
  }
  return false;
}

// Removes from the language list any language that isn't supported as an
// Accept-Language (it's not in kAcceptLanguageList) if and only if there
// aren't any other languages from the same family in the list that are
// supported.
void PurgeUnsupportedLanguagesInLanguageFamily(base::StringPiece language,
                                               std::vector<std::string>* list) {
  base::StringPiece base_language = language::ExtractBaseLanguage(language);
  for (const auto& lang : *list) {
    // This method only operates on languages in the same family as |language|.
    if (base_language != language::ExtractBaseLanguage(lang))
      continue;
    // If at least one of these same-family languages in |list| is supported by
    // Accept-Languages, then that means that none of the languages in this
    // family should be purged.
    if (TranslateAcceptLanguages::CanBeAcceptLanguage(lang))
      return;
  }

  // Purge all languages in the same family as |language|.
  base::EraseIf(*list, [base_language](const std::string& lang) {
    return base_language == language::ExtractBaseLanguage(lang);
  });
}

}  // namespace

const char kForceTriggerTranslateCount[] =
    "translate_force_trigger_on_english_count_for_backoff_1";
const char TranslatePrefs::kPrefTranslateSiteBlacklistDeprecated[] =
    "translate_site_blacklist";
const char TranslatePrefs::kPrefTranslateSiteBlacklistWithTime[] =
    "translate_site_blacklist_with_time";
const char TranslatePrefs::kPrefTranslateWhitelists[] = "translate_whitelists";
const char TranslatePrefs::kPrefTranslateDeniedCount[] =
    "translate_denied_count_for_language";
const char TranslatePrefs::kPrefTranslateIgnoredCount[] =
    "translate_ignored_count_for_language";
const char TranslatePrefs::kPrefTranslateAcceptedCount[] =
    "translate_accepted_count";
const char TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage[] =
    "translate_last_denied_time_for_language";
const char TranslatePrefs::kPrefTranslateTooOftenDeniedForLanguage[] =
    "translate_too_often_denied_for_language";
const char TranslatePrefs::kPrefTranslateRecentTarget[] =
    "translate_recent_target";

#if defined(OS_ANDROID) || defined(OS_IOS)
const char TranslatePrefs::kPrefTranslateAutoAlwaysCount[] =
    "translate_auto_always_count";
const char TranslatePrefs::kPrefTranslateAutoNeverCount[] =
    "translate_auto_never_count";
#endif

#if defined(OS_ANDROID)
const char TranslatePrefs::kPrefExplicitLanguageAskShown[] =
    "translate_explicit_language_ask_shown";
#endif

// The below properties used to be used but now are deprecated. Don't use them
// since an old profile might have some values there.
//
// * translate_last_denied_time
// * translate_too_often_denied
// * translate_language_blacklist

const base::Feature kTranslateRecentTarget{"TranslateRecentTarget",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTranslate{"Translate", base::FEATURE_ENABLED_BY_DEFAULT};

DenialTimeUpdate::DenialTimeUpdate(PrefService* prefs,
                                   base::StringPiece language,
                                   size_t max_denial_count)
    : denial_time_dict_update_(
          prefs,
          TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage),
      language_(language),
      max_denial_count_(max_denial_count),
      time_list_(nullptr) {}

DenialTimeUpdate::~DenialTimeUpdate() {}

// Gets the list of timestamps when translation was denied.
base::ListValue* DenialTimeUpdate::GetDenialTimes() {
  if (time_list_)
    return time_list_;

  // Any consumer of GetDenialTimes _will_ write to them, so let's get an
  // update started.
  base::DictionaryValue* denial_time_dict = denial_time_dict_update_.Get();
  DCHECK(denial_time_dict);

  base::Value* denial_value = nullptr;
  bool has_value = denial_time_dict->Get(language_, &denial_value);
  bool has_list = has_value && denial_value->GetAsList(&time_list_);

  if (!has_list) {
    auto time_list = std::make_unique<base::ListValue>();
    double oldest_denial_time = 0;
    bool has_old_style =
        has_value && denial_value->GetAsDouble(&oldest_denial_time);
    if (has_old_style)
      time_list->AppendDouble(oldest_denial_time);
    time_list_ = denial_time_dict->SetList(language_, std::move(time_list));
  }
  return time_list_;
}

base::Time DenialTimeUpdate::GetOldestDenialTime() {
  double oldest_time;
  bool result = GetDenialTimes()->GetDouble(0, &oldest_time);
  if (!result)
    return base::Time();
  return base::Time::FromJsTime(oldest_time);
}

void DenialTimeUpdate::AddDenialTime(base::Time denial_time) {
  DCHECK(GetDenialTimes());
  GetDenialTimes()->AppendDouble(denial_time.ToJsTime());

  while (GetDenialTimes()->GetSize() >= max_denial_count_)
    GetDenialTimes()->Remove(0, nullptr);
}

TranslateLanguageInfo::TranslateLanguageInfo() = default;

TranslateLanguageInfo::TranslateLanguageInfo(const TranslateLanguageInfo&) =
    default;
TranslateLanguageInfo::TranslateLanguageInfo(TranslateLanguageInfo&&) noexcept =
    default;
TranslateLanguageInfo& TranslateLanguageInfo::operator=(
    const TranslateLanguageInfo&) = default;
TranslateLanguageInfo& TranslateLanguageInfo::operator=(
    TranslateLanguageInfo&&) noexcept = default;

TranslatePrefs::TranslatePrefs(PrefService* user_prefs,
                               const char* accept_languages_pref,
                               const char* preferred_languages_pref)
    : accept_languages_pref_(accept_languages_pref),
      prefs_(user_prefs),
      language_prefs_(std::make_unique<language::LanguagePrefs>(user_prefs)) {
#if defined(OS_CHROMEOS)
  preferred_languages_pref_ = preferred_languages_pref;
#else
  DCHECK(!preferred_languages_pref);
#endif
  MigrateSitesBlacklist();
  ResetEmptyBlockedLanguagesToDefaults();
}

TranslatePrefs::~TranslatePrefs() = default;

bool TranslatePrefs::IsOfferTranslateEnabled() const {
  return prefs_->GetBoolean(prefs::kOfferTranslateEnabled);
}

bool TranslatePrefs::IsTranslateAllowedByPolicy() const {
  const PrefService::Preference* const pref =
      prefs_->FindPreference(prefs::kOfferTranslateEnabled);
  DCHECK(pref);
  DCHECK(pref->GetValue()->is_bool());

  return pref->GetValue()->GetBool() || !pref->IsManaged();
}

void TranslatePrefs::SetCountry(const std::string& country) {
  country_ = country;
}

std::string TranslatePrefs::GetCountry() const {
  return country_;
}

void TranslatePrefs::ResetToDefaults() {
  ResetBlockedLanguagesToDefault();
  ClearBlacklistedSites();
  ClearWhitelistedLanguagePairs();
  prefs_->ClearPref(kPrefTranslateDeniedCount);
  prefs_->ClearPref(kPrefTranslateIgnoredCount);
  prefs_->ClearPref(kPrefTranslateAcceptedCount);
  prefs_->ClearPref(kPrefTranslateRecentTarget);

#if defined(OS_ANDROID) || defined(OS_IOS)
  prefs_->ClearPref(kPrefTranslateAutoAlwaysCount);
  prefs_->ClearPref(kPrefTranslateAutoNeverCount);
#endif

  prefs_->ClearPref(kPrefTranslateLastDeniedTimeForLanguage);
  prefs_->ClearPref(kPrefTranslateTooOftenDeniedForLanguage);

  prefs_->ClearPref(prefs::kOfferTranslateEnabled);
}

bool TranslatePrefs::IsBlockedLanguage(base::StringPiece input_language) const {
  return language_prefs_->IsFluent(input_language);
}

// Note: the language codes used in the language settings list have the Chrome
// internal format and not the Translate server format.
// To convert from one to the other use util functions
// ToTranslateLanguageSynonym() and ToChromeLanguageSynonym().
void TranslatePrefs::AddToLanguageList(base::StringPiece input_language,
                                       const bool force_blocked) {
  DCHECK(!input_language.empty());

  std::string chrome_language(input_language);
  language::ToChromeLanguageSynonym(&chrome_language);

  std::vector<std::string> languages;
  GetLanguageList(&languages);

  // We should block the language if the list does not already contain another
  // language with the same base language.
  const bool should_block =
      !ContainsSameBaseLanguage(languages, chrome_language);

  if (force_blocked || should_block) {
    BlockLanguage(input_language);
  }

  // Add the language to the list.
  if (!base::Contains(languages, chrome_language)) {
    languages.push_back(chrome_language);
    UpdateLanguageList(languages);
  }
}

void TranslatePrefs::RemoveFromLanguageList(base::StringPiece input_language) {
  DCHECK(!input_language.empty());

  std::string chrome_language(input_language);
  language::ToChromeLanguageSynonym(&chrome_language);

  std::vector<std::string> languages;
  GetLanguageList(&languages);

  // Remove the language from the list.
  const auto& it =
      std::find(languages.begin(), languages.end(), chrome_language);
  if (it != languages.end()) {
    // If the language being removed is the most recent language, erase that
    // data so that Chrome won't try to translate to it next time Translate is
    // triggered.
    if (chrome_language == GetRecentTargetLanguage())
      ResetRecentTargetLanguage();

    languages.erase(it);
    PurgeUnsupportedLanguagesInLanguageFamily(chrome_language, &languages);
    UpdateLanguageList(languages);

    // We should unblock the language if this was the last one from the same
    // language family.
    if (!ContainsSameBaseLanguage(languages, chrome_language)) {
      UnblockLanguage(input_language);
    }
  }
}

void TranslatePrefs::RearrangeLanguage(
    base::StringPiece language,
    const TranslatePrefs::RearrangeSpecifier where,
    int offset,
    const std::vector<std::string>& enabled_languages) {
  // Negative offset is not supported.
  DCHECK(!(offset < 1 && (where == kUp || where == kDown)));

  std::vector<std::string> languages;
  GetLanguageList(&languages);

  auto pos = std::find(languages.begin(), languages.end(), language);
  if (pos == languages.end())
    return;

  // Sort the vector of enabled languages for fast lookup.
  std::vector<base::StringPiece> enabled(enabled_languages.begin(),
                                         enabled_languages.end());
  std::sort(enabled.begin(), enabled.end());
  if (!std::binary_search(enabled.begin(), enabled.end(), language))
    return;

  switch (where) {
    case kTop:
      // To avoid code duplication, set |offset| to max int and re-use the logic
      // to move |language| up in the list as far as possible.
      offset = std::numeric_limits<int>::max();
      FALLTHROUGH;
    case kUp:
      if (pos == languages.begin())
        return;
      while (pos != languages.begin()) {
        auto next_pos = pos - 1;
        // Skip over non-enabled languages without decrementing |offset|.
        if (std::binary_search(enabled.begin(), enabled.end(), *next_pos)) {
          // By only checking |offset| when an enabled language is found, and
          // decrementing |offset| after checking it (instead of before), this
          // means that |language| will be moved up the list until it has either
          // reached the next enabled language or the top of the list.
          if (offset <= 0)
            break;
          --offset;
        }
        std::swap(*next_pos, *pos);
        pos = next_pos;
      }
      break;

    case kDown:
      if (pos + 1 == languages.end())
        return;
      for (auto next_pos = pos + 1; next_pos != languages.end() && offset > 0;
           pos = next_pos++) {
        // Skip over non-enabled languages without decrementing offset. Unlike
        // moving languages up in the list, moving languages down in the list
        // stops as soon as |offset| reaches zero, instead of continuing to skip
        // non-enabled languages after |offset| has reached zero.
        if (std::binary_search(enabled.begin(), enabled.end(), *next_pos))
          --offset;
        std::swap(*next_pos, *pos);
      }
      break;

    case kNone:
      return;

    default:
      NOTREACHED();
      return;
  }

  UpdateLanguageList(languages);
}

void TranslatePrefs::SetLanguageOrder(
    const std::vector<std::string>& new_order) {
  UpdateLanguageList(new_order);
}

// static
void TranslatePrefs::GetLanguageInfoList(
    const std::string& app_locale,
    bool translate_allowed,
    std::vector<TranslateLanguageInfo>* language_list) {
  DCHECK(language_list != nullptr);

  if (app_locale.empty()) {
    return;
  }

  language_list->clear();

  // Collect the language codes from the supported accept-languages.
  std::vector<std::string> language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &language_codes);

  // Collator used to sort display names in the given locale.
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(icu::Locale(app_locale.c_str()), error));
  if (U_FAILURE(error)) {
    collator.reset();
  }
  // Map of [display name -> language code].
  std::map<base::string16, std::string,
           l10n_util::StringComparator<base::string16>>
      language_map(l10n_util::StringComparator<base::string16>(collator.get()));

  // Build the list of display names and the language map.
  for (std::string& code : language_codes) {
    language_map[l10n_util::GetDisplayNameForLocale(code, app_locale, false)] =
        std::move(code);
  }

  // Get the sorted list of translatable languages.
  std::vector<std::string> translate_languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      translate_allowed, &translate_languages);
  // |translate_languages| should already be sorted alphabetically for fast
  // searching.
  DCHECK(
      std::is_sorted(translate_languages.begin(), translate_languages.end()));

  // Build the language list from the language map.
  for (auto& entry : language_map) {
    TranslateLanguageInfo language;
    language.code = std::move(entry.second);

    base::string16 adjusted_display_name = entry.first;
    base::i18n::AdjustStringForLocaleDirection(&adjusted_display_name);
    language.display_name = base::UTF16ToUTF8(adjusted_display_name);

    base::string16 adjusted_native_display_name =
        l10n_util::GetDisplayNameForLocale(language.code, language.code, false);
    base::i18n::AdjustStringForLocaleDirection(&adjusted_native_display_name);
    language.native_display_name =
        base::UTF16ToUTF8(adjusted_native_display_name);

    std::string supports_translate_code = language.code;

    // Extract the base language: if the base language can be translated, then
    // even the regional one should be marked as such.
    language::ToTranslateLanguageSynonym(&supports_translate_code);
    language.supports_translate =
        std::binary_search(translate_languages.begin(),
                           translate_languages.end(), supports_translate_code);

    language_list->push_back(std::move(language));
  }
}

void TranslatePrefs::BlockLanguage(base::StringPiece input_language) {
  DCHECK(!input_language.empty());
  language_prefs_->SetFluent(input_language);
}

void TranslatePrefs::UnblockLanguage(base::StringPiece input_language) {
  DCHECK(!input_language.empty());
  language_prefs_->ClearFluent(input_language);
}

bool TranslatePrefs::IsSiteBlacklisted(base::StringPiece site) const {
  return prefs_->GetDictionary(kPrefTranslateSiteBlacklistWithTime)
      ->FindKey(site);
}

void TranslatePrefs::BlacklistSite(base::StringPiece site) {
  DCHECK(!site.empty());
  BlacklistValue(kPrefTranslateSiteBlacklistDeprecated, site);
  DictionaryPrefUpdate update(prefs_, kPrefTranslateSiteBlacklistWithTime);
  base::DictionaryValue* dict = update.Get();
  dict->SetKey(site, util::TimeToValue(base::Time::Now()));
}

void TranslatePrefs::RemoveSiteFromBlacklist(base::StringPiece site) {
  DCHECK(!site.empty());
  RemoveValueFromBlacklist(kPrefTranslateSiteBlacklistDeprecated, site);
  DictionaryPrefUpdate update(prefs_, kPrefTranslateSiteBlacklistWithTime);
  base::DictionaryValue* dict = update.Get();
  dict->RemoveKey(site);
}

std::vector<std::string> TranslatePrefs::GetBlacklistedSitesBetween(
    base::Time begin,
    base::Time end) const {
  std::vector<std::string> result;
  auto* dict = prefs_->GetDictionary(kPrefTranslateSiteBlacklistWithTime);
  for (const auto& entry : dict->DictItems()) {
    base::Optional<base::Time> time = util::ValueToTime(entry.second);
    if (!time) {
      NOTREACHED();
      continue;
    }
    if (begin <= *time && *time < end)
      result.push_back(entry.first);
  }
  return result;
}

void TranslatePrefs::DeleteBlacklistedSitesBetween(base::Time begin,
                                                   base::Time end) {
  for (auto& site : GetBlacklistedSitesBetween(begin, end))
    RemoveSiteFromBlacklist(site);
}

bool TranslatePrefs::IsLanguagePairWhitelisted(
    base::StringPiece original_language,
    base::StringPiece target_language) {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateWhitelists);
  if (dict) {
    const std::string* auto_target_lang =
        dict->FindStringKey(original_language);
    if (auto_target_lang && *auto_target_lang == target_language)
      return true;
  }
  return false;
}

void TranslatePrefs::WhitelistLanguagePair(base::StringPiece original_language,
                                           base::StringPiece target_language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateWhitelists);
  base::DictionaryValue* dict = update.Get();
  if (!dict) {
    NOTREACHED() << "Unregistered translate whitelist pref";
    return;
  }
  dict->SetStringKey(original_language, target_language);
}

void TranslatePrefs::RemoveLanguagePairFromWhitelist(
    base::StringPiece original_language,
    base::StringPiece target_language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateWhitelists);
  base::DictionaryValue* dict = update.Get();
  if (!dict) {
    NOTREACHED() << "Unregistered translate whitelist pref";
    return;
  }
  dict->RemoveKey(original_language);
}

void TranslatePrefs::ResetBlockedLanguagesToDefault() {
  language_prefs_->ResetFluentLanguagesToDefaults();
}

void TranslatePrefs::ClearBlacklistedSites() {
  prefs_->ClearPref(kPrefTranslateSiteBlacklistDeprecated);
  prefs_->ClearPref(kPrefTranslateSiteBlacklistWithTime);
}

bool TranslatePrefs::HasWhitelistedLanguagePairs() const {
  return !IsDictionaryEmpty(kPrefTranslateWhitelists);
}

void TranslatePrefs::ClearWhitelistedLanguagePairs() {
  prefs_->ClearPref(kPrefTranslateWhitelists);
}

int TranslatePrefs::GetTranslationDeniedCount(
    base::StringPiece language) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateDeniedCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationDeniedCount(
    base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  base::DictionaryValue* dict = update.Get();

  int count = 0;
  dict->GetInteger(language, &count);
  if (count < std::numeric_limits<int>::max())
    dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationDeniedCount(base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  update.Get()->SetInteger(language, 0);
}

int TranslatePrefs::GetTranslationIgnoredCount(
    base::StringPiece language) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateIgnoredCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationIgnoredCount(
    base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateIgnoredCount);
  base::DictionaryValue* dict = update.Get();

  int count = 0;
  dict->GetInteger(language, &count);
  if (count < std::numeric_limits<int>::max())
    dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationIgnoredCount(base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateIgnoredCount);
  update.Get()->SetInteger(language, 0);
}

int TranslatePrefs::GetTranslationAcceptedCount(
    base::StringPiece language) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateAcceptedCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationAcceptedCount(
    base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  base::DictionaryValue* dict = update.Get();
  int count = 0;
  dict->GetInteger(language, &count);
  if (count < std::numeric_limits<int>::max())
    dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationAcceptedCount(base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  update.Get()->SetInteger(language, 0);
}

#if defined(OS_ANDROID) || defined(OS_IOS)
int TranslatePrefs::GetTranslationAutoAlwaysCount(
    base::StringPiece language) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateAutoAlwaysCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationAutoAlwaysCount(
    base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAutoAlwaysCount);
  base::DictionaryValue* dict = update.Get();
  int count = 0;
  dict->GetInteger(language, &count);
  if (count < std::numeric_limits<int>::max())
    dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationAutoAlwaysCount(
    base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAutoAlwaysCount);
  update.Get()->SetInteger(language, 0);
}

int TranslatePrefs::GetTranslationAutoNeverCount(
    base::StringPiece language) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateAutoNeverCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationAutoNeverCount(
    base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAutoNeverCount);
  base::DictionaryValue* dict = update.Get();
  int count = 0;
  dict->GetInteger(language, &count);
  if (count < std::numeric_limits<int>::max())
    dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationAutoNeverCount(
    base::StringPiece language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAutoNeverCount);
  update.Get()->SetInteger(language, 0);
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_ANDROID)
bool TranslatePrefs::GetExplicitLanguageAskPromptShown() const {
  return prefs_->GetBoolean(kPrefExplicitLanguageAskShown);
}

void TranslatePrefs::SetExplicitLanguageAskPromptShown(bool shown) {
  prefs_->SetBoolean(kPrefExplicitLanguageAskShown, shown);
}
#endif  // defined(OS_ANDROID)

void TranslatePrefs::UpdateLastDeniedTime(base::StringPiece language) {
  if (IsTooOftenDenied(language))
    return;

  DenialTimeUpdate update(prefs_, language, 2);
  base::Time now = base::Time::Now();
  base::Time oldest_denial_time = update.GetOldestDenialTime();
  update.AddDenialTime(now);

  if (oldest_denial_time.is_null())
    return;

  if (now - oldest_denial_time <= base::TimeDelta::FromDays(1)) {
    DictionaryPrefUpdate update(prefs_,
                                kPrefTranslateTooOftenDeniedForLanguage);
    update.Get()->SetBoolean(language, true);
  }
}

bool TranslatePrefs::IsTooOftenDenied(base::StringPiece language) const {
  return prefs_->GetDictionary(kPrefTranslateTooOftenDeniedForLanguage)
      ->FindBoolPath(language)
      .value_or(false);
}

void TranslatePrefs::ResetDenialState() {
  prefs_->ClearPref(kPrefTranslateLastDeniedTimeForLanguage);
  prefs_->ClearPref(kPrefTranslateTooOftenDeniedForLanguage);
}

void TranslatePrefs::GetLanguageList(
    std::vector<std::string>* const languages) const {
  DCHECK(languages);
  DCHECK(languages->empty());

#if defined(OS_CHROMEOS)
  const std::string& key = preferred_languages_pref_;
#else
  const std::string& key = accept_languages_pref_;
#endif

  *languages = base::SplitString(prefs_->GetString(key), ",",
                                 base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

void TranslatePrefs::UpdateLanguageList(
    const std::vector<std::string>& languages) {
  std::string languages_str = base::JoinString(languages, ",");

#if defined(OS_CHROMEOS)
  prefs_->SetString(preferred_languages_pref_, languages_str);
#endif

  prefs_->SetString(accept_languages_pref_, languages_str);
}

bool TranslatePrefs::CanTranslateLanguage(
    TranslateAcceptLanguages* accept_languages,
    base::StringPiece language) {
  // Don't translate any user black-listed languages.
  if (!IsBlockedLanguage(language))
    return true;

  // Checking |is_accept_language| is necessary because if the user eliminates
  // the language from the preference, it is natural to forget whether or not
  // the language should be translated. Checking |cannot_be_accept_language| is
  // also necessary because some minor languages can't be selected in the
  // language preference even though the language is available in Translate
  // server.
  bool can_be_accept_language =
      TranslateAcceptLanguages::CanBeAcceptLanguage(language);
  bool is_accept_language = accept_languages->IsAcceptLanguage(language);
  if (!is_accept_language && can_be_accept_language)
    return true;

  // Under this experiment, translate English page even though English may be
  // blocked.
  if (language == "en" && language::ShouldForceTriggerTranslateOnEnglishPages(
                              GetForceTriggerOnEnglishPagesCount()))
    return true;
  return false;
}

bool TranslatePrefs::ShouldAutoTranslate(base::StringPiece original_language,
                                         std::string* target_language) {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateWhitelists);
  if (dict && dict->GetString(original_language, target_language)) {
    DCHECK(!target_language->empty());
    return !target_language->empty();
  }
  return false;
}

void TranslatePrefs::SetRecentTargetLanguage(
    const std::string& target_language) {
  prefs_->SetString(kPrefTranslateRecentTarget, target_language);
}

void TranslatePrefs::ResetRecentTargetLanguage() {
  SetRecentTargetLanguage("");
}

std::string TranslatePrefs::GetRecentTargetLanguage() const {
  return prefs_->GetString(kPrefTranslateRecentTarget);
}

int TranslatePrefs::GetForceTriggerOnEnglishPagesCount() const {
  return prefs_->GetInteger(kForceTriggerTranslateCount);
}

void TranslatePrefs::ReportForceTriggerOnEnglishPages() {
  int current_count = GetForceTriggerOnEnglishPagesCount();
  if (current_count != -1 && current_count < std::numeric_limits<int>::max())
    prefs_->SetInteger(kForceTriggerTranslateCount, current_count + 1);
}

void TranslatePrefs::ReportAcceptedAfterForceTriggerOnEnglishPages() {
  int current_count = GetForceTriggerOnEnglishPagesCount();
  if (current_count != -1)
    prefs_->SetInteger(kForceTriggerTranslateCount, -1);
}

// static
void TranslatePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPrefTranslateSiteBlacklistDeprecated,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateSiteBlacklistWithTime,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateWhitelists,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateDeniedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateIgnoredCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateAcceptedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(kPrefTranslateLastDeniedTimeForLanguage);
  registry->RegisterDictionaryPref(
      kPrefTranslateTooOftenDeniedForLanguage,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(kPrefTranslateRecentTarget, "",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kForceTriggerTranslateCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

#if defined(OS_ANDROID) || defined(OS_IOS)
  registry->RegisterDictionaryPref(
      kPrefTranslateAutoAlwaysCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateAutoNeverCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif

#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      kPrefExplicitLanguageAskShown, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
}

void TranslatePrefs::MigrateSitesBlacklist() {
  // Migration should only be necessary once but there could still be old
  // Chrome instances that sync the old preference, so do it once per
  // startup.
  static bool migrated = false;
  if (migrated)
    return;
  DictionaryPrefUpdate blacklist_update(prefs_,
                                        kPrefTranslateSiteBlacklistWithTime);
  base::DictionaryValue* blacklist = blacklist_update.Get();
  if (blacklist) {
    const base::ListValue* list =
        prefs_->GetList(kPrefTranslateSiteBlacklistDeprecated);
    for (auto& site : *list) {
      if (!blacklist->HasKey(site.GetString())) {
        blacklist->SetKey(site.GetString(), base::Value(0));
      }
    }
  }
  migrated = true;
}

void TranslatePrefs::ResetEmptyBlockedLanguagesToDefaults() {
  language_prefs_->ResetEmptyFluentLanguagesToDefault();
}

bool TranslatePrefs::IsValueBlacklisted(const char* pref_id,
                                        base::StringPiece value) const {
  const base::ListValue* blacklist = prefs_->GetList(pref_id);
  if (!blacklist)
    return false;
  for (const base::Value& value_in_list : blacklist->GetList()) {
    if (value_in_list.is_string() && value_in_list.GetString() == value)
      return true;
  }
  return false;
}

void TranslatePrefs::BlacklistValue(const char* pref_id,
                                    base::StringPiece value) {
  ListPrefUpdate update(prefs_, pref_id);
  base::ListValue* blacklist = update.Get();
  if (!blacklist) {
    NOTREACHED() << "Unregistered translate blacklist pref";
    return;
  }

  if (IsValueBlacklisted(pref_id, value)) {
    return;
  }
  blacklist->AppendString(value);
}

void TranslatePrefs::RemoveValueFromBlacklist(const char* pref_id,
                                              base::StringPiece value) {
  ListPrefUpdate update(prefs_, pref_id);
  base::ListValue* blacklist = update.Get();
  if (!blacklist) {
    NOTREACHED() << "Unregistered translate blacklist pref";
    return;
  }

  auto list_view = blacklist->GetList();
  blacklist->EraseListIter(std::find_if(
      list_view.begin(), list_view.end(),
      [value](const base::Value& value_in_list) {
        return value_in_list.is_string() && value_in_list.GetString() == value;
      }));
}

size_t TranslatePrefs::GetListSize(const char* pref_id) const {
  const base::ListValue* blacklist = prefs_->GetList(pref_id);
  return blacklist == nullptr ? 0 : blacklist->GetList().size();
}

bool TranslatePrefs::IsDictionaryEmpty(const char* pref_id) const {
  const base::DictionaryValue* dict = prefs_->GetDictionary(pref_id);
  return (dict == nullptr || dict->empty());
}

}  // namespace translate
