// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_prefs.h"

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

namespace translate {

namespace {

const int kForceTriggerBackoffThreshold = 4;

// Returns whether or not the given list includes at least one language with
// the same base as the input language.
// For example: "en-US" and "en-UK" share the same base "en".
bool ContainsSameBaseLanguage(const std::vector<std::string>& list,
                              std::string_view language_code) {
  std::string_view base_language = language::ExtractBaseLanguage(language_code);
  for (const auto& item : list) {
    if (base_language == language::ExtractBaseLanguage(item))
      return true;
  }
  return false;
}

// Merge old always-translate languages from the deprecated pref to the new
// version. Because of crbug/1291356, it's possible that on iOS the client
// started using the new pref without properly migrating and clearing the old
// pref. This function will avoid merging values from the old pref that seem to
// conflict with values already present in the new pref.
void MigrateObsoleteAlwaysTranslateLanguagesPref(PrefService* prefs) {
  const base::Value::Dict& deprecated_dictionary =
      prefs->GetDict(TranslatePrefs::kPrefAlwaysTranslateListDeprecated);
  // Migration is performed only once per client, since the deprecated pref is
  // cleared after migration. This will make subsequent calls to migrate no-ops.
  if (deprecated_dictionary.empty())
    return;

  ScopedDictPrefUpdate always_translate_dictionary_update(
      prefs, prefs::kPrefAlwaysTranslateList);
  base::Value::Dict& always_translate_dictionary =
      always_translate_dictionary_update.Get();

  for (const auto old_language_pair : deprecated_dictionary) {
    // If the old pref's language pair conflicts with any of the new pref's
    // language pairs, where either the new pref already specifies behavior
    // about always translating from or to the old source language, or always
    // translating from the old target language, then skip merging this pair
    // into the new pref.
    if (base::ranges::any_of(
            always_translate_dictionary,
            [&old_language_pair](const auto& new_language_pair) {
              return old_language_pair.first == new_language_pair.first ||
                     old_language_pair.first ==
                         new_language_pair.second.GetString() ||
                     old_language_pair.second.GetString() ==
                         new_language_pair.first;
            })) {
      continue;
    }

    // If the old pair's source language matches any of the never-translate
    // languages, it probably means that this source language was set to never
    // be translated after the old pref was deprecated, so avoid this conflict.
    const std::string& (base::Value::*get_string)() const =
        &base::Value::GetString;
    if (base::Contains(prefs->GetList(prefs::kBlockedLanguages),
                       old_language_pair.first, get_string)) {
      continue;
    }

    always_translate_dictionary.Set(old_language_pair.first,
                                    old_language_pair.second.GetString());
  }

  prefs->ClearPref(TranslatePrefs::kPrefAlwaysTranslateListDeprecated);
}

bool IsTranslateLanguage(std::string_view language) {
  // Allow "xx" to be used for testing purposes.
  if (language == "xx") {
    return true;
  }

  // Check if |language| is translatable.
  TranslateLanguageList* language_list =
      TranslateDownloadManager::GetInstance()->language_list();
  return language_list && language_list->IsSupportedLanguage(language);
}
}  // namespace

// The below properties used to be used but now are deprecated. Don't use them
// since an old profile might have some values there.
//
// * translate_last_denied_time
// * translate_too_often_denied
// * translate_language_blacklist

BASE_FEATURE(kTranslateRecentTarget,
             "TranslateRecentTarget",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTranslate, "Translate", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMigrateAlwaysTranslateLanguagesFix,
             "MigrateAlwaysTranslateLanguagesFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

TranslateLanguageInfo::TranslateLanguageInfo() = default;

TranslateLanguageInfo::TranslateLanguageInfo(const TranslateLanguageInfo&) =
    default;
TranslateLanguageInfo::TranslateLanguageInfo(TranslateLanguageInfo&&) noexcept =
    default;
TranslateLanguageInfo& TranslateLanguageInfo::operator=(
    const TranslateLanguageInfo&) = default;
TranslateLanguageInfo& TranslateLanguageInfo::operator=(
    TranslateLanguageInfo&&) noexcept = default;

TranslatePrefs::TranslatePrefs(PrefService* user_prefs)
    : prefs_(user_prefs),
      language_prefs_(std::make_unique<language::LanguagePrefs>(user_prefs)) {
  MigrateNeverPromptSites();
  ResetEmptyBlockedLanguagesToDefaults();

  if (base::FeatureList::IsEnabled(kMigrateAlwaysTranslateLanguagesFix))
    MigrateObsoleteAlwaysTranslateLanguagesPref(user_prefs);
}

TranslatePrefs::~TranslatePrefs() = default;

// static
std::string TranslatePrefs::MapPreferenceName(const std::string& pref_name) {
  if (pref_name == kPrefNeverPromptSitesDeprecated) {
    return "translate_site_blocklist";
  }
  return pref_name;
}

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
  ClearNeverPromptSiteList();
  ClearAlwaysTranslateLanguagePairs();
  prefs_->ClearPref(kPrefTranslateDeniedCount);
  prefs_->ClearPref(kPrefTranslateIgnoredCount);
  prefs_->ClearPref(kPrefTranslateAcceptedCount);
  prefs_->ClearPref(prefs::kPrefTranslateRecentTarget);
  force_translate_on_english_for_testing_ = false;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  prefs_->ClearPref(kPrefTranslateAutoAlwaysCount);
  prefs_->ClearPref(kPrefTranslateAutoNeverCount);
#endif

  prefs_->ClearPref(prefs::kOfferTranslateEnabled);
}

// static
base::Value::List TranslatePrefs::GetDefaultBlockedLanguages() {
  base::Value::List languages;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Preferred languages.
  std::string language = language::kFallbackInputMethodLocale;
  language::ToTranslateLanguageSynonym(&language);
  languages.Append(std::move(language));
#else
  // Accept languages.
#pragma GCC diagnostic push
// See comment above the |break;| in the loop just below for why.
#pragma GCC diagnostic ignored "-Wunreachable-code"
  for (std::string& language :
       base::SplitString(l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    language::ToTranslateLanguageSynonym(&language);
    languages.Append(std::move(language));

    // crbug.com/958348: The default value for Accept-Language *should* be the
    // same as the one for Blocked Languages. However, Accept-Language contains
    // English (and more) in addition to the local language in most locales due
    // to historical reasons. Exiting early from this loop is a temporary fix
    // that allows Blocked Languages to be at least populated with the UI
    // language while still allowing Translate to trigger on other languages,
    // most importantly English.
    // Once the change to remove English from Accept-Language defaults lands,
    // this break should be removed to enable the Blocked Language List and the
    // Accept-Language list to be initialized to the same values.
    break;
#pragma GCC diagnostic pop
  }
#endif

  std::sort(languages.begin(), languages.end());
  languages.erase(std::unique(languages.begin(), languages.end()),
                  languages.end());

  return languages;
}

bool TranslatePrefs::IsBlockedLanguage(std::string_view input_language) const {
  std::string canonical_lang(input_language);
  language::ToTranslateLanguageSynonym(&canonical_lang);
  const base::Value::List& blocked =
      prefs_->GetList(translate::prefs::kBlockedLanguages);
  return base::Contains(blocked, base::Value(std::move(canonical_lang)));
}

void TranslatePrefs::BlockLanguage(std::string_view input_language) {
  DCHECK(!input_language.empty());
  std::string canonical_lang(input_language);
  language::ToTranslateLanguageSynonym(&canonical_lang);
  if (!l10n_util::IsPossibleAcceptLanguage(canonical_lang)) {
    return;
  }

  if (!IsBlockedLanguage(canonical_lang)) {
    ScopedListPrefUpdate update(prefs_, translate::prefs::kBlockedLanguages);
    update->Append(std::move(canonical_lang));
  }
  // Remove the blocked language from the always translate list if present.
  RemoveLanguagePairFromAlwaysTranslateList(input_language);
}

void TranslatePrefs::UnblockLanguage(std::string_view input_language) {
  DCHECK(!input_language.empty());
  // Never remove last fluent language.
  if (GetNeverTranslateLanguages().size() <= 1) {
    return;
  }
  std::string canonical_lang(input_language);
  language::ToTranslateLanguageSynonym(&canonical_lang);
  ScopedListPrefUpdate update(prefs_, translate::prefs::kBlockedLanguages);
  update->EraseValue(base::Value(std::move(canonical_lang)));
}

void TranslatePrefs::ResetEmptyBlockedLanguagesToDefaults() {
  if (GetNeverTranslateLanguages().size() == 0) {
    ResetBlockedLanguagesToDefault();
  }
}

void TranslatePrefs::ResetBlockedLanguagesToDefault() {
  prefs_->ClearPref(translate::prefs::kBlockedLanguages);
}

std::vector<std::string> TranslatePrefs::GetNeverTranslateLanguages() const {
  const base::Value::List& fluent_languages_value =
      prefs_->GetList(translate::prefs::kBlockedLanguages);

  std::vector<std::string> languages;
  for (const auto& language : fluent_languages_value) {
    std::string chrome_language(language.GetString());
    language::ToChromeLanguageSynonym(&chrome_language);
    languages.push_back(chrome_language);
  }
  return languages;
}

// Note: the language codes used in the language settings list have the Chrome
// internal format and not the Translate server format.
// To convert from one to the other use util functions
// ToTranslateLanguageSynonym() and ToChromeLanguageSynonym().
void TranslatePrefs::AddToLanguageList(std::string_view input_language,
                                       const bool force_blocked) {
  DCHECK(!input_language.empty());

  std::string chrome_language(input_language);
  language::ToChromeLanguageSynonym(&chrome_language);

  std::vector<std::string> languages;
  std::vector<std::string> user_selected_languages;
  GetLanguageList(&languages);
  GetUserSelectedLanguageList(&user_selected_languages);

  // We should block the language if the list does not already contain another
  // language with the same base language. Policy-forced languages aren't
  // counted as "blocking", so only user-selected languages are checked.
  const bool should_block =
      !ContainsSameBaseLanguage(user_selected_languages, chrome_language);

  if (force_blocked || should_block) {
    BlockLanguage(input_language);
  }

  // Add the language to the list.
  if (!base::Contains(languages, chrome_language)) {
    user_selected_languages.push_back(chrome_language);
    language_prefs_->SetUserSelectedLanguagesList(user_selected_languages);
  }
}

void TranslatePrefs::RemoveFromLanguageList(std::string_view input_language) {
  DCHECK(!input_language.empty());

  std::string chrome_language(input_language);
  language::ToChromeLanguageSynonym(&chrome_language);

  std::vector<std::string> languages;
  std::vector<std::string> user_selected_languages;
  GetUserSelectedLanguageList(&user_selected_languages);

  // Remove the language from the list.
  const auto& it = base::ranges::find(user_selected_languages, chrome_language);
  if (it != user_selected_languages.end()) {

    user_selected_languages.erase(it);
    language_prefs_->SetUserSelectedLanguagesList(user_selected_languages);

    // We should unblock the language if this was the last one from the same
    // language family.
    GetLanguageList(&languages);
    if (!ContainsSameBaseLanguage(languages, chrome_language)) {
      UnblockLanguage(input_language);
      // If the recent translate target matches the last language of a family
      // being removed, reset the most recent target language so it will not be
      // used the next time Translate is triggered.
      std::string translate_language(input_language);
      language::ToTranslateLanguageSynonym(&translate_language);
      if (translate_language == GetRecentTargetLanguage()) {
        ResetRecentTargetLanguage();
      }
    }
  }
}

void TranslatePrefs::RearrangeLanguage(
    std::string_view language,
    const TranslatePrefs::RearrangeSpecifier where,
    int offset,
    const std::vector<std::string>& enabled_languages) {
  // Negative offset is not supported.
  DCHECK(!(offset < 1 && (where == kUp || where == kDown)));

  std::vector<std::string> languages;
  GetUserSelectedLanguageList(&languages);

  auto pos = base::ranges::find(languages, language);
  if (pos == languages.end())
    return;

  // Sort the vector of enabled languages for fast lookup.
  std::vector<std::string_view> enabled(enabled_languages.begin(),
                                        enabled_languages.end());
  std::sort(enabled.begin(), enabled.end());
  if (!std::binary_search(enabled.begin(), enabled.end(), language))
    return;

  switch (where) {
    case kTop:
      // To avoid code duplication, set |offset| to max int and re-use the logic
      // to move |language| up in the list as far as possible.
      offset = std::numeric_limits<int>::max();
      [[fallthrough]];
    case kUp:
      if (pos == languages.begin())
        return;
      while (pos != languages.begin()) {
        auto next_pos = pos - 1;
        // Skip over non-enabled languages without decrementing |offset|.
        // Also skip over languages hidden due to duplication between forced
        // and user-selected languages.
        if (std::binary_search(enabled.begin(), enabled.end(), *next_pos) &&
            !language_prefs_->IsForcedLanguage(*next_pos)) {
          // By only checking |offset| when an enabled, non-forced language is
          // found, and decrementing |offset| after checking it (instead of
          // before), this means that |language| will be moved up the list until
          // it has either reached the next enabled language or the top of the
          // list.
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
        // Skip over non-enabled or forced languages without decrementing
        // offset. Unlike moving languages up in the list, moving languages down
        // in the list stops as soon as |offset| reaches zero, instead of
        // continuing to skip non-enabled languages after |offset| has reached
        // zero.
        if (std::binary_search(enabled.begin(), enabled.end(), *next_pos) &&
            !language_prefs_->IsForcedLanguage(*next_pos))
          --offset;
        std::swap(*next_pos, *pos);
      }
      break;

    case kNone:
      return;

    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  language_prefs_->SetUserSelectedLanguagesList(languages);
}

void TranslatePrefs::SetLanguageOrder(
    const std::vector<std::string>& new_order) {
  language_prefs_->SetUserSelectedLanguagesList(new_order);
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
  std::map<std::u16string, std::string,
           l10n_util::StringComparator<std::u16string>>
      language_map(l10n_util::StringComparator<std::u16string>(collator.get()));

  // Build the list of display names and the language map.
  for (std::string& code : language_codes) {
    language_map[l10n_util::GetDisplayNameForLocale(code, app_locale, false)] =
        std::move(code);
  }

  if (translate_allowed) {
    translate::TranslateDownloadManager::RequestLanguageList();
  }

  // Build the language list from the language map.
  for (auto& entry : language_map) {
    TranslateLanguageInfo language;
    language.code = std::move(entry.second);

    std::u16string adjusted_display_name = entry.first;
    base::i18n::AdjustStringForLocaleDirection(&adjusted_display_name);
    language.display_name = base::UTF16ToUTF8(adjusted_display_name);

    std::u16string adjusted_native_display_name =
        l10n_util::GetDisplayNameForLocale(language.code, language.code, false);
    base::i18n::AdjustStringForLocaleDirection(&adjusted_native_display_name);
    language.native_display_name =
        base::UTF16ToUTF8(adjusted_native_display_name);

    // Extract the base language: if the base language can be translated, then
    // even the regional one should be marked as such.
    std::string translate_code = language.code;
    language::ToTranslateLanguageSynonym(&translate_code);
    language.supports_translate =
        translate::TranslateDownloadManager::IsSupportedLanguage(
            translate_code);
    language_list->push_back(std::move(language));
  }
}

void TranslatePrefs::GetTranslatableContentLanguages(
    const std::string& app_locale,
    std::vector<std::string>* codes) {
  DCHECK(codes != nullptr);

  if (app_locale.empty()) {
    return;
  }
  codes->clear();

  // Get the language codes of user content languages.
  // Returned in Chrome format.
  std::vector<std::string> language_codes;
  GetLanguageList(&language_codes);

  std::set<std::string> unique_languages;
  for (auto& entry : language_codes) {
    std::string supports_translate_code = entry;
    // Get the language in Translate format.
    language::ToTranslateLanguageSynonym(&supports_translate_code);
    // Extract the language code, for example for en-US it returns en.
    std::string lang_code =
        TranslateDownloadManager::GetLanguageCode(supports_translate_code);
    // If the language code for a translatable language hasn't yet been added,
    // add it to the result list.
    if (TranslateDownloadManager::IsSupportedLanguage(lang_code)) {
      if (unique_languages.count(lang_code) == 0) {
        unique_languages.insert(lang_code);
        codes->push_back(lang_code);
      }
    }
  }
}

bool TranslatePrefs::IsSiteOnNeverPromptList(std::string_view site) const {
  return prefs_->GetDict(prefs::kPrefNeverPromptSitesWithTime).Find(site);
}

void TranslatePrefs::AddSiteToNeverPromptList(std::string_view site,
                                              base::Time time) {
  DCHECK(!site.empty());
  AddValueToNeverPromptList(kPrefNeverPromptSitesDeprecated, site);
  ScopedDictPrefUpdate update(prefs_, prefs::kPrefNeverPromptSitesWithTime);
  update->Set(site, base::TimeToValue(time));
}

void TranslatePrefs::AddSiteToNeverPromptList(std::string_view site) {
  AddSiteToNeverPromptList(site, base::Time::Now());
}

void TranslatePrefs::RemoveSiteFromNeverPromptList(std::string_view site) {
  DCHECK(!site.empty());
  RemoveValueFromNeverPromptList(kPrefNeverPromptSitesDeprecated, site);
  ScopedDictPrefUpdate update(prefs_, prefs::kPrefNeverPromptSitesWithTime);
  update->Remove(site);
}

std::vector<std::string> TranslatePrefs::GetNeverPromptSitesBetween(
    base::Time begin,
    base::Time end) const {
  std::vector<std::string> result;
  const auto& dict = prefs_->GetDict(prefs::kPrefNeverPromptSitesWithTime);
  for (const auto entry : dict) {
    std::optional<base::Time> time = base::ValueToTime(entry.second);
    if (!time) {
      // Badly formatted preferences may be synced from the server, see
      // https://crbug.com/1295549
      LOG(ERROR) << "Preference " << prefs::kPrefNeverPromptSitesWithTime
                 << " has invalid format. Ignoring.";
      continue;
    }
    if (begin <= *time && *time < end)
      result.push_back(entry.first);
  }
  return result;
}

void TranslatePrefs::DeleteNeverPromptSitesBetween(base::Time begin,
                                                   base::Time end) {
  for (auto& site : GetNeverPromptSitesBetween(begin, end))
    RemoveSiteFromNeverPromptList(site);
}

bool TranslatePrefs::IsLanguagePairOnAlwaysTranslateList(
    std::string_view source_language,
    std::string_view target_language) {
  const base::Value::Dict& dict =
      prefs_->GetDict(prefs::kPrefAlwaysTranslateList);

  const std::string* auto_target_lang = dict.FindString(source_language);
  if (auto_target_lang && *auto_target_lang == target_language)
    return true;

  return false;
}

void TranslatePrefs::AddLanguagePairToAlwaysTranslateList(
    std::string_view source_language,
    std::string_view target_language) {
  // Get translate version of language codes.
  std::string translate_source_language(source_language);
  language::ToTranslateLanguageSynonym(&translate_source_language);
  std::string translate_target_language(target_language);
  language::ToTranslateLanguageSynonym(&translate_target_language);
  if (!IsTranslateLanguage(translate_source_language) ||
      !IsTranslateLanguage(translate_target_language)) {
    return;
  }

  ScopedDictPrefUpdate update(prefs_, prefs::kPrefAlwaysTranslateList);

  update->Set(translate_source_language, translate_target_language);
  // Remove source language from block list if present.
  UnblockLanguage(translate_source_language);
}

void TranslatePrefs::RemoveLanguagePairFromAlwaysTranslateList(
    std::string_view source_language) {
  ScopedDictPrefUpdate update(prefs_, prefs::kPrefAlwaysTranslateList);

  // Get translate version of language codes.
  std::string translate_source_language(source_language);
  language::ToTranslateLanguageSynonym(&translate_source_language);
  update->Remove(translate_source_language);
}

std::vector<std::string> TranslatePrefs::GetAlwaysTranslateLanguages() const {
  const base::Value::Dict& dict =
      prefs_->GetDict(prefs::kPrefAlwaysTranslateList);

  std::vector<std::string> languages;
  for (auto language_pair : dict) {
    std::string chrome_language(language_pair.first);
    language::ToChromeLanguageSynonym(&chrome_language);
    languages.push_back(chrome_language);
  }
  return languages;
}

void TranslatePrefs::ClearNeverPromptSiteList() {
  prefs_->ClearPref(kPrefNeverPromptSitesDeprecated);
  prefs_->ClearPref(prefs::kPrefNeverPromptSitesWithTime);
}

bool TranslatePrefs::HasLanguagePairsToAlwaysTranslate() const {
  return !IsDictionaryEmpty(prefs::kPrefAlwaysTranslateList);
}

void TranslatePrefs::ClearAlwaysTranslateLanguagePairs() {
  prefs_->ClearPref(prefs::kPrefAlwaysTranslateList);
}

int TranslatePrefs::GetTranslationDeniedCount(std::string_view language) const {
  const base::Value::Dict& dict = prefs_->GetDict(kPrefTranslateDeniedCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationDeniedCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  base::Value::Dict& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationDeniedCount(std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  update->Set(language, 0);
}

int TranslatePrefs::GetTranslationIgnoredCount(
    std::string_view language) const {
  const base::Value::Dict& dict = prefs_->GetDict(kPrefTranslateIgnoredCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationIgnoredCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateIgnoredCount);
  base::Value::Dict& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationIgnoredCount(std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateIgnoredCount);
  update->Set(language, 0);
}

int TranslatePrefs::GetTranslationAcceptedCount(
    std::string_view language) const {
  const base::Value::Dict& dict = prefs_->GetDict(kPrefTranslateAcceptedCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationAcceptedCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  base::Value::Dict& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationAcceptedCount(std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  update->Set(language, 0);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
int TranslatePrefs::GetTranslationAutoAlwaysCount(
    std::string_view language) const {
  const base::Value::Dict& dict =
      prefs_->GetDict(kPrefTranslateAutoAlwaysCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationAutoAlwaysCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAutoAlwaysCount);
  base::Value::Dict& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationAutoAlwaysCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAutoAlwaysCount);
  update->Set(language, 0);
}

int TranslatePrefs::GetTranslationAutoNeverCount(
    std::string_view language) const {
  const base::Value::Dict& dict = prefs_->GetDict(kPrefTranslateAutoNeverCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationAutoNeverCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAutoNeverCount);
  base::Value::Dict& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationAutoNeverCount(std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAutoNeverCount);
  update->Set(language, 0);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
bool TranslatePrefs::GetAppLanguagePromptShown() const {
  return prefs_->GetBoolean(language::prefs::kAppLanguagePromptShown);
}

void TranslatePrefs::SetAppLanguagePromptShown() {
  prefs_->SetBoolean(language::prefs::kAppLanguagePromptShown, true);
}
#endif  // BUILDFLAG(IS_ANDROID)

void TranslatePrefs::GetLanguageList(
    std::vector<std::string>* const languages) const {
  language_prefs_->GetAcceptLanguagesList(languages);
}

void TranslatePrefs::GetUserSelectedLanguageList(
    std::vector<std::string>* const languages) const {
  language_prefs_->GetUserSelectedLanguagesList(languages);
}

bool TranslatePrefs::ShouldForceTriggerTranslateOnEnglishPages() {
  if (!language::OverrideTranslateTriggerInIndia() &&
      !force_translate_on_english_for_testing_) {
    return false;
  }

  return GetForceTriggerOnEnglishPagesCount() < kForceTriggerBackoffThreshold;
}

bool TranslatePrefs::force_translate_on_english_for_testing_ = false;

// static
void TranslatePrefs::SetShouldForceTriggerTranslateOnEnglishPagesForTesting() {
  force_translate_on_english_for_testing_ = true;
}

bool TranslatePrefs::CanTranslateLanguage(std::string_view language) {
  // Under this experiment, translate English page even though English may be
  // blocked.
  if (language == "en" && ShouldForceTriggerTranslateOnEnglishPages()) {
    return true;
  }

  return !IsBlockedLanguage(language);
}

bool TranslatePrefs::ShouldAutoTranslate(std::string_view source_language,
                                         std::string* target_language) {
  const base::Value::Dict& dict =
      prefs_->GetDict(prefs::kPrefAlwaysTranslateList);

  const std::string* value = dict.FindString(source_language);
  if (!value)
    return false;

  DCHECK(!value->empty());
  target_language->assign(*value);
  return true;
}

void TranslatePrefs::SetRecentTargetLanguage(
    const std::string& target_language) {
  // Get translate version of language code.
  std::string translate_target_language(target_language);
  language::ToTranslateLanguageSynonym(&translate_target_language);
  prefs_->SetString(prefs::kPrefTranslateRecentTarget,
                    translate_target_language);
}

void TranslatePrefs::ResetRecentTargetLanguage() {
  SetRecentTargetLanguage("");
}

std::string TranslatePrefs::GetRecentTargetLanguage() const {
  return prefs_->GetString(prefs::kPrefTranslateRecentTarget);
}

int TranslatePrefs::GetForceTriggerOnEnglishPagesCount() const {
  return prefs_->GetInteger(kPrefForceTriggerTranslateCount);
}

void TranslatePrefs::ReportForceTriggerOnEnglishPages() {
  int current_count = GetForceTriggerOnEnglishPagesCount();
  if (current_count != -1 && current_count < std::numeric_limits<int>::max())
    prefs_->SetInteger(kPrefForceTriggerTranslateCount, current_count + 1);
}

void TranslatePrefs::ReportAcceptedAfterForceTriggerOnEnglishPages() {
  int current_count = GetForceTriggerOnEnglishPagesCount();
  if (current_count != -1)
    prefs_->SetInteger(kPrefForceTriggerTranslateCount, -1);
}

// static
void TranslatePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPrefNeverPromptSitesDeprecated);
  registry->RegisterDictionaryPref(
      prefs::kPrefNeverPromptSitesWithTime,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kPrefAlwaysTranslateList,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateDeniedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(kPrefTranslateIgnoredCount);
  registry->RegisterDictionaryPref(
      kPrefTranslateAcceptedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kPrefTranslateRecentTarget, "",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kPrefForceTriggerTranslateCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(translate::prefs::kBlockedLanguages,
                             TranslatePrefs::GetDefaultBlockedLanguages(),
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  registry->RegisterDictionaryPref(
      kPrefTranslateAutoAlwaysCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateAutoNeverCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif

  RegisterProfilePrefsForMigration(registry);
}

// static
void TranslatePrefs::RegisterProfilePrefsForMigration(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(crbug.com/40826252): Deprecated 10/2021. Check status of bug before
  // removing.
  registry->RegisterDictionaryPref(kPrefAlwaysTranslateListDeprecated);
}

void TranslatePrefs::MigrateNeverPromptSites() {
  // Migration copies any sites on the deprecated never prompt pref to
  // the new version and clears all references to the old one. This will
  // make subsequent calls to migrate no-ops.

  // Use of ScopedDictPrefUpdate is avoided since a call to its Get() ensures
  // that the observers are notified upon destruction no matter if the value was
  // changed or not.
  base::Value::Dict never_prompt_list =
      prefs_->GetDict(prefs::kPrefNeverPromptSitesWithTime).Clone();
  ScopedListPrefUpdate deprecated_prompt_list_update(
      prefs_, kPrefNeverPromptSitesDeprecated);
  base::Value::List& deprecated_list = deprecated_prompt_list_update.Get();
  for (auto& site : deprecated_list) {
    if (site.is_string() &&
        (!never_prompt_list.Find(site.GetString()) ||
         !base::ValueToTime(never_prompt_list.Find(site.GetString())))) {
      never_prompt_list.Set(site.GetString(),
                            base::TimeToValue(base::Time::Now()));
    }
  }
  deprecated_list.clear();
  prefs_->SetDict(prefs::kPrefNeverPromptSitesWithTime,
                  std::move(never_prompt_list));
}

bool TranslatePrefs::IsValueOnNeverPromptList(const char* pref_id,
                                              std::string_view value) const {
  const base::Value::List& never_prompt_list = prefs_->GetList(pref_id);
  for (const base::Value& value_in_list : never_prompt_list) {
    if (value_in_list.is_string() && value_in_list.GetString() == value)
      return true;
  }
  return false;
}

void TranslatePrefs::AddValueToNeverPromptList(const char* pref_id,
                                               std::string_view value) {
  ScopedListPrefUpdate update(prefs_, pref_id);
  base::Value::List& never_prompt_list = update.Get();

  if (IsValueOnNeverPromptList(pref_id, value)) {
    return;
  }
  never_prompt_list.Append(value);
}

void TranslatePrefs::RemoveValueFromNeverPromptList(const char* pref_id,
                                                    std::string_view value) {
  ScopedListPrefUpdate update(prefs_, pref_id);
  base::Value::List& never_prompt_list = update.Get();

  auto value_to_erase = base::ranges::find_if(
      never_prompt_list, [value](const base::Value& value_in_list) {
        return value_in_list.is_string() && value_in_list.GetString() == value;
      });
  if (value_to_erase != never_prompt_list.end())
    never_prompt_list.erase(value_to_erase);
}

size_t TranslatePrefs::GetListSize(const char* pref_id) const {
  const base::Value::List& never_prompt_list = prefs_->GetList(pref_id);
  return never_prompt_list.size();
}

bool TranslatePrefs::IsDictionaryEmpty(const char* pref_id) const {
  const base::Value::Dict& dict = prefs_->GetDict(pref_id);
  return (dict.empty());
}
}  // namespace translate
