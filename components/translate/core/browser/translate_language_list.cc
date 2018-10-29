// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_language_list.h"

#include <stddef.h>

#include <set>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
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
// Server also supports "hmm" (Hmong) and "jw" (Javanese), but these are
// excluded because Chrome l10n library does not support it.
const char* const kDefaultSupportedLanguages[] = {
  "af",     // Afrikaans
  "am",     // Amharic
  "ar",     // Arabic
  "az",     // Azerbaijani
  "be",     // Belarusian
  "bg",     // Bulgarian
  "bn",     // Bengali
  "bs",     // Bosnian
  "ca",     // Catalan
  "ceb",    // Cebuano
  "co",     // Corsican
  "cs",     // Czech
  "cy",     // Welsh
  "da",     // Danish
  "de",     // German
  "el",     // Greek
  "en",     // English
  "eo",     // Esperanto
  "es",     // Spanish
  "et",     // Estonian
  "eu",     // Basque
  "fa",     // Persian
  "fi",     // Finnish
  "fy",     // Frisian
  "fr",     // French
  "ga",     // Irish
  "gd",     // Scots Gaelic
  "gl",     // Galician
  "gu",     // Gujarati
  "ha",     // Hausa
  "haw",    // Hawaiian
  "hi",     // Hindi
  "hr",     // Croatian
  "ht",     // Haitian Creole
  "hu",     // Hungarian
  "hy",     // Armenian
  "id",     // Indonesian
  "ig",     // Igbo
  "is",     // Icelandic
  "it",     // Italian
  "iw",     // Hebrew
  "ja",     // Japanese
  "ka",     // Georgian
  "kk",     // Kazakh
  "km",     // Khmer
  "kn",     // Kannada
  "ko",     // Korean
  "ku",     // Kurdish
  "ky",     // Kyrgyz
  "la",     // Latin
  "lb",     // Luxembourgish
  "lo",     // Lao
  "lt",     // Lithuanian
  "lv",     // Latvian
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
  "no",     // Norwegian
  "ny",     // Nyanja
  "pa",     // Punjabi
  "pl",     // Polish
  "ps",     // Pashto
  "pt",     // Portuguese
  "ro",     // Romanian
  "ru",     // Russian
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
  "tl",     // Tagalog
  "tr",     // Turkish
  "uk",     // Ukrainian
  "ur",     // Urdu
  "uz",     // Uzbek
  "vi",     // Vietnamese
  "yi",     // Yiddish
  "xh",     // Xhosa
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
    : resource_requests_allowed_(false), request_pending_(false) {
  // We default to our hard coded list of languages in
  // |kDefaultSupportedLanguages|. This list will be overridden by a server
  // providing supported languages list.
  for (size_t i = 0; i < arraysize(kDefaultSupportedLanguages); ++i)
    supported_languages_.insert(kDefaultSupportedLanguages[i]);

  if (update_is_disabled)
    return;

  language_list_fetcher_.reset(new TranslateURLFetcher);
  language_list_fetcher_->set_max_retry_on_5xx(kMaxRetryOn5xx);
}

TranslateLanguageList::~TranslateLanguageList() {}

void TranslateLanguageList::GetSupportedLanguages(
    bool translate_allowed,
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  auto iter = supported_languages_.begin();
  for (; iter != supported_languages_.end(); ++iter)
    languages->push_back(*iter);

  // Update language lists if they are not updated after Chrome was launched
  // for later requests.
  if (translate_allowed && !update_is_disabled && language_list_fetcher_.get())
    RequestLanguageList();
}

std::string TranslateLanguageList::GetLanguageCode(
    const std::string& language) {
  // Only remove the country code for country specific languages we don't
  // support specifically yet.
  if (IsSupportedLanguage(language))
    return language;

  size_t hypen_index = language.find('-');
  if (hypen_index == std::string::npos)
    return language;
  return language.substr(0, hypen_index);
}

bool TranslateLanguageList::IsSupportedLanguage(const std::string& language) {
  return supported_languages_.count(language) != 0;
}

GURL TranslateLanguageList::TranslateLanguageUrl() {
  std::string url = translate::GetTranslateSecurityOrigin().spec() +
      kLanguageListFetchPath;
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

    std::string message = base::StringPrintf(
        "Language list fetch starts (URL: %s)", url.spec().c_str());
    NotifyEvent(__LINE__, message);

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

std::unique_ptr<TranslateLanguageList::EventCallbackList::Subscription>
TranslateLanguageList::RegisterEventCallback(const EventCallback& callback) {
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

void TranslateLanguageList::NotifyEvent(int line, const std::string& message) {
  TranslateEventDetails details(__FILE__, line, message);
  callback_list_.Notify(details);
}

bool TranslateLanguageList::SetSupportedLanguages(
    const std::string& language_list) {
  // The format is in JSON as:
  // {
  //   "sl": {"XX": "LanguageName", ...},
  //   "tl": {"XX": "LanguageName", ...}
  // }
  // Where "tl" is set in kTargetLanguagesKey.
  std::unique_ptr<base::Value> json_value =
      base::JSONReader::Read(language_list, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!json_value || !json_value->is_dict()) {
    NotifyEvent(__LINE__, "Language list is invalid");
    NOTREACHED();
    return false;
  }
  // The first level dictionary contains three sub-dict, first for source
  // languages and second for target languages, we want to use the target
  // languages.
  base::DictionaryValue* language_dict =
      static_cast<base::DictionaryValue*>(json_value.get());
  base::DictionaryValue* target_languages = nullptr;
  if (!language_dict->GetDictionary(TranslateLanguageList::kTargetLanguagesKey,
                                    &target_languages) ||
      target_languages == nullptr) {
    NotifyEvent(__LINE__, "Target languages are not found in the response");
    NOTREACHED();
    return false;
  }

  const std::string& locale =
      TranslateDownloadManager::GetInstance()->application_locale();

  // Now we can clear language list.
  supported_languages_.clear();
  std::string message;
  // ... and replace it with the values we just fetched from the server.
  for (base::DictionaryValue::Iterator iter(*target_languages);
       !iter.IsAtEnd();
       iter.Advance()) {
    const std::string& lang = iter.key();
    if (!l10n_util::IsLocaleNameTranslated(lang.c_str(), locale)) {
      TranslateBrowserMetrics::ReportUndisplayableLanguage(lang);
      continue;
    }
    supported_languages_.insert(lang);
    if (message.empty())
      message += lang;
    else
      message += ", " + lang;
  }
  NotifyEvent(__LINE__, message);
  return true;
}

}  // namespace translate
