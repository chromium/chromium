// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_language_list.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
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
#include "components/translate/core/common/translate_language_matcher.h"
#include "components/translate/core/common/translate_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace translate {
namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;
using ::base::i18n::LanguageTagConverter;

// The default list of languages the Partial Translation service supports.
// This list must be sorted in alphabetical order and contain no duplicates.
// This list is identical to above except that it excludes
// {ilo lus mni-Mtei gom doi bm ckb}.
constexpr auto kDefaultSupportedPartialTranslateLanguages =
    base::MakeFixedFlatSet<LanguageTag>({
        GetKnownLanguageTag("af"),     // Afrikaans
        GetKnownLanguageTag("ak"),     // Akan
        GetKnownLanguageTag("am"),     // Amharic
        GetKnownLanguageTag("ar"),     // Arabic
        GetKnownLanguageTag("as"),     // Assamese
        GetKnownLanguageTag("ay"),     // Aymara
        GetKnownLanguageTag("az"),     // Azerbaijani
        GetKnownLanguageTag("be"),     // Belarusian
        GetKnownLanguageTag("bg"),     // Bulgarian
        GetKnownLanguageTag("bho"),    // Bhojpuri
        GetKnownLanguageTag("bn"),     // Bengali
        GetKnownLanguageTag("bs"),     // Bosnian
        GetKnownLanguageTag("ca"),     // Catalan
        GetKnownLanguageTag("ceb"),    // Cebuano
        GetKnownLanguageTag("co"),     // Corsican
        GetKnownLanguageTag("cs"),     // Czech
        GetKnownLanguageTag("cy"),     // Welsh
        GetKnownLanguageTag("da"),     // Danish
        GetKnownLanguageTag("de"),     // German
        GetKnownLanguageTag("dv"),     // Dhivehi
        GetKnownLanguageTag("ee"),     // Ewe
        GetKnownLanguageTag("el"),     // Greek
        GetKnownLanguageTag("en"),     // English
        GetKnownLanguageTag("eo"),     // Esperanto
        GetKnownLanguageTag("es"),     // Spanish
        GetKnownLanguageTag("et"),     // Estonian
        GetKnownLanguageTag("eu"),     // Basque
        GetKnownLanguageTag("fa"),     // Persian
        GetKnownLanguageTag("fi"),     // Finnish
        GetKnownLanguageTag("fil"),    // Filipino
        GetKnownLanguageTag("fr"),     // French
        GetKnownLanguageTag("fy"),     // Frisian
        GetKnownLanguageTag("ga"),     // Irish
        GetKnownLanguageTag("gd"),     // Scots Gaelic
        GetKnownLanguageTag("gl"),     // Galician
        GetKnownLanguageTag("gu"),     // Gujarati
        GetKnownLanguageTag("ha"),     // Hausa
        GetKnownLanguageTag("haw"),    // Hawaiian
        GetKnownLanguageTag("he"),     // Hebrew
        GetKnownLanguageTag("hi"),     // Hindi
        GetKnownLanguageTag("hmn"),    // Hmong
        GetKnownLanguageTag("hr"),     // Croatian
        GetKnownLanguageTag("ht"),     // Haitian Creole
        GetKnownLanguageTag("hu"),     // Hungarian
        GetKnownLanguageTag("hy"),     // Armenian
        GetKnownLanguageTag("id"),     // Indonesian
        GetKnownLanguageTag("ig"),     // Igbo
        GetKnownLanguageTag("is"),     // Icelandic
        GetKnownLanguageTag("it"),     // Italian
        GetKnownLanguageTag("ja"),     // Japanese
        GetKnownLanguageTag("jv"),     // Javanese
        GetKnownLanguageTag("ka"),     // Georgian
        GetKnownLanguageTag("kk"),     // Kazakh
        GetKnownLanguageTag("km"),     // Khmer
        GetKnownLanguageTag("kn"),     // Kannada
        GetKnownLanguageTag("ko"),     // Korean
        GetKnownLanguageTag("kri"),    // Krio
        GetKnownLanguageTag("ku"),     // Kurdish (Kurmanji)
        GetKnownLanguageTag("ky"),     // Kyrgyz
        GetKnownLanguageTag("la"),     // Latin
        GetKnownLanguageTag("lb"),     // Luxembourgish
        GetKnownLanguageTag("lg"),     // Luganda
        GetKnownLanguageTag("ln"),     // Lingala
        GetKnownLanguageTag("lo"),     // Lao
        GetKnownLanguageTag("lt"),     // Lithuanian
        GetKnownLanguageTag("lv"),     // Latvian
        GetKnownLanguageTag("mai"),    // Maithili
        GetKnownLanguageTag("mg"),     // Malagasy
        GetKnownLanguageTag("mi"),     // Maori
        GetKnownLanguageTag("mk"),     // Macedonian
        GetKnownLanguageTag("ml"),     // Malayalam
        GetKnownLanguageTag("mn"),     // Mongolian
        GetKnownLanguageTag("mr"),     // Marathi
        GetKnownLanguageTag("ms"),     // Malay
        GetKnownLanguageTag("mt"),     // Maltese
        GetKnownLanguageTag("my"),     // Myanmar (Burmese)
        GetKnownLanguageTag("ne"),     // Nepali
        GetKnownLanguageTag("nl"),     // Dutch
        GetKnownLanguageTag("no"),     // Norwegian
        GetKnownLanguageTag("nso"),    // Sepedi
        GetKnownLanguageTag("ny"),     // Chichewa
        GetKnownLanguageTag("om"),     // Oromo
        GetKnownLanguageTag("or"),     // Odia (Oriya)
        GetKnownLanguageTag("pa"),     // Punjabi
        GetKnownLanguageTag("pl"),     // Polish
        GetKnownLanguageTag("ps"),     // Pashto
        GetKnownLanguageTag("pt"),     // Portuguese
        GetKnownLanguageTag("qu"),     // Quechua
        GetKnownLanguageTag("ro"),     // Romanian
        GetKnownLanguageTag("ru"),     // Russian
        GetKnownLanguageTag("rw"),     // Kinyarwanda
        GetKnownLanguageTag("sa"),     // Sanskrit
        GetKnownLanguageTag("sd"),     // Sindhi
        GetKnownLanguageTag("si"),     // Sinhala
        GetKnownLanguageTag("sk"),     // Slovak
        GetKnownLanguageTag("sl"),     // Slovenian
        GetKnownLanguageTag("sm"),     // Samoan
        GetKnownLanguageTag("sn"),     // Shona
        GetKnownLanguageTag("so"),     // Somali
        GetKnownLanguageTag("sq"),     // Albanian
        GetKnownLanguageTag("sr"),     // Serbian
        GetKnownLanguageTag("st"),     // Sesotho
        GetKnownLanguageTag("su"),     // Sundanese
        GetKnownLanguageTag("sv"),     // Swedish
        GetKnownLanguageTag("sw"),     // Swahili
        GetKnownLanguageTag("ta"),     // Tamil
        GetKnownLanguageTag("te"),     // Telugu
        GetKnownLanguageTag("tg"),     // Tajik
        GetKnownLanguageTag("th"),     // Thai
        GetKnownLanguageTag("ti"),     // Tigrinya
        GetKnownLanguageTag("tk"),     // Turkmen
        GetKnownLanguageTag("tr"),     // Turkish
        GetKnownLanguageTag("ts"),     // Tsonga
        GetKnownLanguageTag("tt"),     // Tatar
        GetKnownLanguageTag("ug"),     // Uyghur
        GetKnownLanguageTag("uk"),     // Ukrainian
        GetKnownLanguageTag("ur"),     // Urdu
        GetKnownLanguageTag("uz"),     // Uzbek
        GetKnownLanguageTag("vi"),     // Vietnamese
        GetKnownLanguageTag("xh"),     // Xhosa
        GetKnownLanguageTag("yi"),     // Yiddish
        GetKnownLanguageTag("yo"),     // Yoruba
        GetKnownLanguageTag("zh-CN"),  // Chinese (Simplified)
        GetKnownLanguageTag("zh-TW"),  // Chinese (Traditional)
        GetKnownLanguageTag("zu"),     // Zulu
    });

// Constant URL string to fetch server supporting language list.
constexpr std::string_view kLanguageListFetchPath =
    "translate_a/l?client=chrome";

// Retry parameter for fetching.
constexpr int kMaxRetryOn5xx = 5;

void SortAndUnique(std::vector<LanguageTag>& languages) {
  std::sort(languages.begin(), languages.end());
  languages.erase(std::unique(languages.begin(), languages.end()),
                  languages.end());
}

}  // namespace

const char TranslateLanguageList::kTargetLanguagesKey[] = "tl";

TranslateLanguageList::TranslateLanguageList()
    : TranslateLanguageList(
          std::make_unique<TranslateURLFetcherImpl>(kMaxRetryOn5xx)) {}

TranslateLanguageList::TranslateLanguageList(
    std::unique_ptr<TranslateUrlFetcher> fetcher)
    : resource_requests_allowed_(false),
      request_pending_(false),
      // We default to our hard coded list of languages in
      // |translate::GetDefaultSupportedLanguages()|. This list will be
      // overridden by a server providing supported languages list.
      supported_languages_(std::from_range,
                           translate::GetDefaultSupportedLanguages()),
      language_list_fetcher_(std::move(fetcher)) {}

TranslateLanguageList::~TranslateLanguageList() = default;

void TranslateLanguageList::GetSupportedLanguages(
    bool translate_allowed,
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  for (const LanguageTag& tag : supported_languages_) {
    languages->emplace_back(tag.tag_string());
  }

  // Update language lists if they are not updated after Chrome was launched
  // for later requests.
  if (translate_allowed && language_list_fetcher_.get()) {
    RequestLanguageList();
  }
}

// static
void TranslateLanguageList::GetSupportedPartialTranslateLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());

  for (const LanguageTag& tag : kDefaultSupportedPartialTranslateLanguages) {
    languages->emplace_back(tag.tag_string());
  }
}

std::string TranslateLanguageList::GetLanguageCode(std::string_view language) {
  // Only remove the country code for country specific languages we don't
  // support specifically yet.
  if (IsSupportedLanguage(language)) {
    return std::string(language);
  }
  return std::string(language::ExtractBaseLanguage(language));
}

bool TranslateLanguageList::IsSupportedLanguage(std::string_view language) {
  std::optional<LanguageTag> tag =
      LanguageTagConverter::GetInstance().FromString(language);
  if (!tag) {
    return false;
  }
  return supported_languages_.contains(*tag);
}

// static
bool TranslateLanguageList::IsSupportedPartialTranslateLanguage(
    std::string_view language) {
  std::optional<LanguageTag> tag =
      LanguageTagConverter::GetInstance().FromString(language);
  if (!tag) {
    return false;
  }
  return kDefaultSupportedPartialTranslateLanguages.contains(*tag);
}

// static
GURL TranslateLanguageList::TranslateLanguageUrl() {
  return GURL(base::StrCat({translate::GetTranslateSecurityOrigin().spec(),
                            kLanguageListFetchPath}));
}

void TranslateLanguageList::RequestLanguageList() {
  // If resource requests are not allowed, we'll get a callback when they are.
  if (!resource_requests_allowed_) {
    request_pending_ = true;
    return;
  }

  request_pending_ = false;

  if (language_list_fetcher_.get() &&
      (language_list_fetcher_->state() == TranslateUrlFetcher::IDLE ||
       language_list_fetcher_->state() == TranslateUrlFetcher::FAILED)) {
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
    if (!result) {
      NotifyEvent(__LINE__, "Request is omitted due to retry limit");
    }
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
  return language_list_fetcher_->state() == TranslateUrlFetcher::REQUESTING;
}

GURL TranslateLanguageList::LanguageFetchURLForTesting() {
  return AddApiKeyToUrl(AddHostLocaleToUrl(TranslateLanguageUrl()));
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

  if (parsed_correctly) {
    last_updated_ = base::Time::Now();
  }
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
  std::optional<base::DictValue> json_value = base::JSONReader::ReadDict(
      language_list, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!json_value) {
    LOG(ERROR) << "Failed to parse language list.";
    // TODO(bug:478219404): Find better way to report this issue.
    return false;
  }
  // The first level dictionary contains two sub-dicts, first for source
  // languages and second for target languages. We want to use the target
  // languages.
  const base::DictValue* target_languages =
      json_value->FindDict(TranslateLanguageList::kTargetLanguagesKey);
  if (!target_languages) {
    LOG(ERROR) << "Target languages not found in translate language list.";
    // TODO(bug:478219404): Find better way to report this issue.
    return false;
  }

  std::vector<LanguageTag> supported_languages_from_service;
  // ... and replace it with the values we just fetched from the server.
  for (auto [lang, language_name] : *target_languages) {
    if (!language::AcceptLanguagesService::CanBeAcceptLanguage(lang.c_str())) {
      // Don't include languages that can not be Accept-Languages
      continue;
    }
    if (std::optional<LanguageTag> language_tag =
            LanguageTagConverter::GetInstance().FromString(lang);
        language_tag) {
      supported_languages_from_service.emplace_back(*language_tag);
    }
  }

  SortAndUnique(supported_languages_from_service);
  supported_languages_ = base::flat_set<LanguageTag>(
      base::sorted_unique, std::move(supported_languages_from_service));

  std::vector<std::string_view> languages_as_strings;
  std::ranges::transform(supported_languages_,
                         std::back_inserter(languages_as_strings),
                         &LanguageTag::tag_string);

  NotifyEvent(__LINE__, base::JoinString(languages_as_strings, ", "));
  return true;
}

}  // namespace translate
