// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_language_list.h"

#include <stddef.h>

#include <iterator>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/common/locale_util.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_event_details.h"
#include "components/translate/core/browser/translate_url_fetcher.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/translate_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace translate {

namespace {

// The default list of languages the Google translation server supports.
// We use this list until we receive the list that the server exposes.
// This list must be sorted in alphabetical order and contain no duplicates.
const char* const kDefaultSupportedLanguages[] = {
    "af",        // Afrikaans
    "ak",        // Twi
    "am",        // Amharic
    "ar",        // Arabic
    "as",        // Assamese
    "ay",        // Aymara
    "az",        // Azerbaijani
    "be",        // Belarusian
    "bg",        // Bulgarian
    "bho",       // Bhojpuri
    "bm",        // Bambara
    "bn",        // Bengali
    "bs",        // Bosnian
    "ca",        // Catalan
    "ceb",       // Cebuano
    "ckb",       // Kurdish (Sorani)
    "co",        // Corsican
    "cs",        // Czech
    "cy",        // Welsh
    "da",        // Danish
    "de",        // German
    "doi",       // Dogri
    "dv",        // Dhivehi
    "ee",        // Ewe
    "el",        // Greek
    "en",        // English
    "eo",        // Esperanto
    "es",        // Spanish
    "et",        // Estonian
    "eu",        // Basque
    "fa",        // Persian
    "fi",        // Finnish
    "fr",        // French
    "fy",        // Frisian
    "ga",        // Irish
    "gd",        // Scots Gaelic
    "gl",        // Galician
    "gom",       // Konkani
    "gu",        // Gujarati
    "ha",        // Hausa
    "haw",       // Hawaiian
    "hi",        // Hindi
    "hmn",       // Hmong
    "hr",        // Croatian
    "ht",        // Haitian Creole
    "hu",        // Hungarian
    "hy",        // Armenian
    "id",        // Indonesian
    "ig",        // Igbo
    "ilo",       // Ilocano
    "is",        // Icelandic
    "it",        // Italian
    "iw",        // Hebrew - Chrome uses "he"
    "ja",        // Japanese
    "jw",        // Javanese - Chrome uses "jv"
    "ka",        // Georgian
    "kk",        // Kazakh
    "km",        // Khmer
    "kn",        // Kannada
    "ko",        // Korean
    "kri",       // Krio
    "ku",        // Kurdish
    "ky",        // Kyrgyz
    "la",        // Latin
    "lb",        // Luxembourgish
    "lg",        // Luganda
    "ln",        // Lingala
    "lo",        // Lao
    "lt",        // Lithuanian
    "lus",       // Mizo
    "lv",        // Latvian
    "mai",       // Maithili
    "mg",        // Malagasy
    "mi",        // Maori
    "mk",        // Macedonian
    "ml",        // Malayalam
    "mn",        // Mongolian
    "mni-Mtei",  // Manipuri (Meitei Mayek)
    "mr",        // Marathi
    "ms",        // Malay
    "mt",        // Maltese
    "my",        // Burmese
    "ne",        // Nepali
    "nl",        // Dutch
    "no",        // Norwegian - Chrome uses "nb"
    "nso",       // Sepedi
    "ny",        // Nyanja
    "om",        // Oromo
    "or",        // Odia (Oriya)
    "pa",        // Punjabi
    "pl",        // Polish
    "ps",        // Pashto
    "pt",        // Portuguese
    "qu",        // Quechua
    "ro",        // Romanian
    "ru",        // Russian
    "rw",        // Kinyarwanda
    "sa",        // Sanskrit
    "sd",        // Sindhi
    "si",        // Sinhala
    "sk",        // Slovak
    "sl",        // Slovenian
    "sm",        // Samoan
    "sn",        // Shona
    "so",        // Somali
    "sq",        // Albanian
    "sr",        // Serbian
    "st",        // Southern Sotho
    "su",        // Sundanese
    "sv",        // Swedish
    "sw",        // Swahili
    "ta",        // Tamil
    "te",        // Telugu
    "tg",        // Tajik
    "th",        // Thai
    "ti",        // Tigrinya
    "tk",        // Turkmen
    "tl",        // Tagalog - Chrome uses "fil"
    "tr",        // Turkish
    "ts",        // Tsonga
    "tt",        // Tatar
    "ug",        // Uyghur
    "uk",        // Ukrainian
    "ur",        // Urdu
    "uz",        // Uzbek
    "vi",        // Vietnamese
    "xh",        // Xhosa
    "yi",        // Yiddish
    "yo",        // Yoruba
    "zh-CN",     // Chinese (Simplified)
    "zh-TW",     // Chinese (Traditional)
    "zu",        // Zulu
};

// The default list of languages the Partial Translation service supports.
// This list must be sorted in alphabetical order and contain no duplicates.
// This list is identical to above except that it excludes
// {ilo lus mni-Mtei gom doi bm ckb}.
const char* const kDefaultSupportedPartialTranslateLanguages[] = {
    "af",     // Afrikaans
    "ak",     // Twi
    "am",     // Amharic
    "ar",     // Arabic
    "as",     // Assamese
    "ay",     // Aymara
    "az",     // Azerbaijani
    "be",     // Belarusian
    "bg",     // Bulgarian
    "bho",    // Bhojpuri
    "bn",     // Bengali
    "bs",     // Bosnian
    "ca",     // Catalan
    "ceb",    // Cebuano
    "co",     // Corsican
    "cs",     // Czech
    "cy",     // Welsh
    "da",     // Danish
    "de",     // German
    "dv",     // Dhivehi
    "ee",     // Ewe
    "el",     // Greek
    "en",     // English
    "eo",     // Esperanto
    "es",     // Spanish
    "et",     // Estonian
    "eu",     // Basque
    "fa",     // Persian
    "fi",     // Finnish
    "fr",     // French
    "fy",     // Frisian
    "ga",     // Irish
    "gd",     // Scots Gaelic
    "gl",     // Galician
    "gu",     // Gujarati
    "ha",     // Hausa
    "haw",    // Hawaiian
    "hi",     // Hindi
    "hmn",    // Hmong
    "hr",     // Croatian
    "ht",     // Haitian Creole
    "hu",     // Hungarian
    "hy",     // Armenian
    "id",     // Indonesian
    "ig",     // Igbo
    "is",     // Icelandic
    "it",     // Italian
    "iw",     // Hebrew - Chrome uses "he"
    "ja",     // Japanese
    "jw",     // Javanese - Chrome uses "jv"
    "ka",     // Georgian
    "kk",     // Kazakh
    "km",     // Khmer
    "kn",     // Kannada
    "ko",     // Korean
    "kri",    // Krio
    "ku",     // Kurdish
    "ky",     // Kyrgyz
    "la",     // Latin
    "lb",     // Luxembourgish
    "lg",     // Luganda
    "ln",     // Lingala
    "lo",     // Lao
    "lt",     // Lithuanian
    "lv",     // Latvian
    "mai",    // Maithili
    "mg",     // Malagasy
    "mi",     // Maori
    "mk",     // Macedonian
    "ml",     // Malayalam
    "mn",     // Mongolian
    "mr",     // Marathi
    "ms",     // Malay
    "mt",     // Maltese
    "my",     // Burmese
    "ne",     // Nepali
    "nl",     // Dutch
    "no",     // Norwegian - Chrome uses "nb"
    "nso",    // Sepedi
    "ny",     // Nyanja
    "om",     // Oromo
    "or",     // Odia (Oriya)
    "pa",     // Punjabi
    "pl",     // Polish
    "ps",     // Pashto
    "pt",     // Portuguese
    "qu",     // Quechua
    "ro",     // Romanian
    "ru",     // Russian
    "rw",     // Kinyarwanda
    "sa",     // Sanskrit
    "sd",     // Sindhi
    "si",     // Sinhala
    "sk",     // Slovak
    "sl",     // Slovenian
    "sm",     // Samoan
    "sn",     // Shona
    "so",     // Somali
    "sq",     // Albanian
    "sr",     // Serbian
    "st",     // Southern Sotho
    "su",     // Sundanese
    "sv",     // Swedish
    "sw",     // Swahili
    "ta",     // Tamil
    "te",     // Telugu
    "tg",     // Tajik
    "th",     // Thai
    "ti",     // Tigrinya
    "tk",     // Turkmen
    "tl",     // Tagalog - Chrome uses "fil"
    "tr",     // Turkish
    "ts",     // Tsonga
    "tt",     // Tatar
    "ug",     // Uyghur
    "uk",     // Ukrainian
    "ur",     // Urdu
    "uz",     // Uzbek
    "vi",     // Vietnamese
    "xh",     // Xhosa
    "yi",     // Yiddish
    "yo",     // Yoruba
    "zh-CN",  // Chinese (Simplified)
    "zh-TW",  // Chinese (Traditional)
    "zu",     // Zulu
};

// Constant URL string to fetch server supporting language list.
const char kLanguageListFetchPath[] = "translate_a/l?client=chrome";

// Represent if the language list updater is disabled.
bool update_is_disabled = false;

// Retry parameter for fetching.
const int kMaxRetryOn5xx = 5;

}  // namespace

const char TranslateLanguageList::kTargetLanguagesKey[] = "tl";

TranslateLanguageList::TranslateLanguageList()
    : resource_requests_allowed_(false),
      request_pending_(false),
      // We default to our hard coded list of languages in
      // |kDefaultSupportedLanguages|. This list will be overridden by a server
      // providing supported languages list.
      supported_languages_(std::begin(kDefaultSupportedLanguages),
                           std::end(kDefaultSupportedLanguages)) {
  // |kDefaultSupportedLanguages| should be sorted alphabetically and contain no
  // duplicates.
  DCHECK(
      std::is_sorted(supported_languages_.begin(), supported_languages_.end()));
  DCHECK(supported_languages_.end() ==
         base::ranges::adjacent_find(supported_languages_));

  if (update_is_disabled)
    return;

  language_list_fetcher_ = std::make_unique<TranslateURLFetcher>();
  language_list_fetcher_->set_max_retry_on_5xx(kMaxRetryOn5xx);
}

TranslateLanguageList::~TranslateLanguageList() {}

void TranslateLanguageList::GetSupportedLanguages(
    bool translate_allowed,
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  *languages = supported_languages_;

  // Update language lists if they are not updated after Chrome was launched
  // for later requests.
  if (translate_allowed && !update_is_disabled &&
      language_list_fetcher_.get()) {
    RequestLanguageList();
  }
}

// static
void TranslateLanguageList::GetSupportedPartialTranslateLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());

  *languages = std::vector<std::string>(
      std::begin(kDefaultSupportedPartialTranslateLanguages),
      std::end(kDefaultSupportedPartialTranslateLanguages));
}

std::string TranslateLanguageList::GetLanguageCode(std::string_view language) {
  // Only remove the country code for country specific languages we don't
  // support specifically yet.
  if (IsSupportedLanguage(language))
    return std::string(language);
  return std::string(language::ExtractBaseLanguage(language));
}

bool TranslateLanguageList::IsSupportedLanguage(std::string_view language) {
  return base::ranges::binary_search(supported_languages_, language);
}

// static
bool TranslateLanguageList::IsSupportedPartialTranslateLanguage(
    std::string_view language) {
  return base::ranges::binary_search(kDefaultSupportedPartialTranslateLanguages,
                                     language);
}

// static
GURL TranslateLanguageList::TranslateLanguageUrl() {
  std::string url =
      translate::GetTranslateSecurityOrigin().spec() + kLanguageListFetchPath;
  return GURL(url);
}

void TranslateLanguageList::RequestLanguageList() {
  // If resource requests are not allowed, we'll get a callback when they are.
  if (!resource_requests_allowed_) {
    request_pending_ = true;
    return;
  }

  request_pending_ = false;

  if (language_list_fetcher_.get() &&
      (language_list_fetcher_->state() == TranslateURLFetcher::IDLE ||
       language_list_fetcher_->state() == TranslateURLFetcher::FAILED)) {
    GURL url = TranslateLanguageUrl();
    url = AddHostLocaleToUrl(url);
    url = AddApiKeyToUrl(url);

    NotifyEvent(__LINE__,
                base::StringPrintf("Language list fetch starts (URL: %s)",
                                   url.spec().c_str()));

    bool result = language_list_fetcher_->Request(
        url,
        base::BindOnce(&TranslateLanguageList::OnLanguageListFetchComplete,
                       base::Unretained(this)),
        // Use the strictest mode for request headers, since incognito state is
        // not known.
        /*is_incognito=*/true);
    if (!result)
      NotifyEvent(__LINE__, "Request is omitted due to retry limit");
  }
}

void TranslateLanguageList::SetResourceRequestsAllowed(bool allowed) {
  resource_requests_allowed_ = allowed;
  if (resource_requests_allowed_ && request_pending_) {
    RequestLanguageList();
    DCHECK(!request_pending_);
  }
}

base::CallbackListSubscription TranslateLanguageList::RegisterEventCallback(
    const EventCallback& callback) {
  return callback_list_.Add(callback);
}

bool TranslateLanguageList::HasOngoingLanguageListLoadingForTesting() {
  return language_list_fetcher_->state() == TranslateURLFetcher::REQUESTING;
}

GURL TranslateLanguageList::LanguageFetchURLForTesting() {
  return AddApiKeyToUrl(AddHostLocaleToUrl(TranslateLanguageUrl()));
}

// static
void TranslateLanguageList::DisableUpdate() {
  update_is_disabled = true;
}

void TranslateLanguageList::OnLanguageListFetchComplete(
    bool success,
    const std::string& data) {
  if (!success) {
    // Since it fails just now, omit to schedule resource requests if
    // ResourceRequestAllowedNotifier think it's ready. Otherwise, a callback
    // will be invoked later to request resources again.
    // The TranslateURLFetcher has a limit for retried requests and aborts
    // re-try not to invoke OnLanguageListFetchComplete anymore if it's asked to
    // re-try too many times.
    NotifyEvent(__LINE__, "Failed to fetch languages");
    return;
  }

  NotifyEvent(__LINE__, "Language list is updated");

  bool parsed_correctly = SetSupportedLanguages(data);
  language_list_fetcher_.reset();

  if (parsed_correctly)
    last_updated_ = base::Time::Now();
}

void TranslateLanguageList::NotifyEvent(int line, std::string message) {
  TranslateEventDetails details(__FILE__, line, std::move(message));
  callback_list_.Notify(details);
}

bool TranslateLanguageList::SetSupportedLanguages(
    std::string_view language_list) {
  // The format is in JSON as:
  // {
  //   "sl": {"XX": "LanguageName", ...},
  //   "tl": {"XX": "LanguageName", ...}
  // }
  // Where "tl" is set in kTargetLanguagesKey.
  std::optional<base::Value> json_value =
      base::JSONReader::Read(language_list, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!json_value || !json_value->is_dict()) {
    NotifyEvent(__LINE__, "Language list is invalid");
    base::debug::DumpWithoutCrashing();
    return false;
  }
  // The first level dictionary contains two sub-dicts, first for source
  // languages and second for target languages. We want to use the target
  // languages.
  const base::Value::Dict* target_languages = json_value->GetDict().FindDict(
      TranslateLanguageList::kTargetLanguagesKey);
  if (!target_languages) {
    NotifyEvent(__LINE__, "Target languages are not found in the response");
    base::debug::DumpWithoutCrashing();
    return false;
  }

  // Now we can clear language list.
  supported_languages_.clear();
  // ... and replace it with the values we just fetched from the server.
  for (auto kv_pair : *target_languages) {
    const std::string& lang = kv_pair.first;
    if (!language::AcceptLanguagesService::CanBeAcceptLanguage(lang.c_str())) {
      // Don't include languages that can not be Accept-Languages
      continue;
    }
    supported_languages_.push_back(lang);
  }

  // Since the DictionaryValue was sorted by key, |supported_languages_| should
  // already be sorted and have no duplicate values.
  DCHECK(
      std::is_sorted(supported_languages_.begin(), supported_languages_.end()));
  DCHECK(supported_languages_.end() ==
         base::ranges::adjacent_find(supported_languages_));

  NotifyEvent(__LINE__, base::JoinString(supported_languages_, ", "));
  return true;
}

}  // namespace translate
