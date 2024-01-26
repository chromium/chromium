// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spelling_service_client.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

// The REST endpoint for requesting spell checking and sending user feedback.
const char kSpellingServiceRestURL[] =
    "https://www.googleapis.com/spelling/v%d/spelling/check?key=%s";

// The spellcheck suggestions object key in the JSON response from the spelling
// service.
const char kMisspellingsRestPath[] = "spellingCheckResponse.misspellings";

// The location of error messages in JSON response from spelling service.
const char kErrorPath[] = "error";

// Languages currently supported by SPELLCHECK.
const char* const kValidLanguages[] = {"en", "es", "fi", "da"};

}  // namespace

SpellingServiceClient::SpellingServiceClient() = default;

SpellingServiceClient::~SpellingServiceClient() = default;

bool SpellingServiceClient::RequestTextCheck(
    content::BrowserContext* context,
    ServiceType type,
    const std::u16string& text,
    TextCheckCompleteCallback callback) {
  DCHECK(type == SUGGEST || type == SPELLCHECK);
  if (!context || !IsAvailable(context, type)) {
    std::move(callback).Run(false, text, std::vector<SpellCheckResult>());
    return false;
  }
  const PrefService* pref = user_prefs::UserPrefs::Get(context);
  DCHECK(pref);

  std::string dictionary;
  const base::Value::List& dicts_list =
      pref->GetList(spellcheck::prefs::kSpellCheckDictionaries);
  if (0u < dicts_list.size() && dicts_list[0].is_string())
    dictionary = dicts_list[0].GetString();

  std::string language_code;
  std::string country_code;
  spellcheck::GetISOLanguageCountryCodeFromLocale(dictionary, &language_code,
                                                  &country_code);

  // Replace typographical apostrophes with typewriter apostrophes, so that
  // server word breaker behaves correctly.
  const char16_t kApostrophe = 0x27;
  const char16_t kRightSingleQuotationMark = 0x2019;
  std::u16string text_copy = text;
  std::replace(text_copy.begin(), text_copy.end(), kRightSingleQuotationMark,
               kApostrophe);

  std::string encoded_text = base::GetQuotedJSONString(text_copy);

  static const char kSpellingRequestRestBodyTemplate[] =
      "{"
      "\"text\":%s,"
      "\"language\":\"%s\","
      "\"originCountry\":\"%s\""
      "}";

  std::string request_body =
      base::StringPrintf(kSpellingRequestRestBodyTemplate, encoded_text.c_str(),
                         language_code.c_str(), country_code.c_str());

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("spellcheck_lookup", R"(
        semantics {
          sender: "Online Spellcheck"
          description:
            "Chromium can provide smarter spell-checking, by sending the text "
            "that the users type into the browser, to Google's servers. This"
            "allows users to use the same spell-checking technology used by "
            "Google products, such as Docs. If the feature is enabled, "
            "Chromium will send the entire contents of text fields as user "
            "types them to Google, along with the browser’s default language. "
            "Google returns a list of suggested spellings, which will be "
            "displayed in the context menu."
          trigger: "User types text into a text field or asks to correct a "
                   "misspelled word."
          data: "Text a user has typed into a text field. No user identifier "
                "is sent along with the text."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature via 'Enhanced spell "
            "check' in Chromium's settings under 'Sync and Google services'. "
            "The feature is disabled by default."
          chrome_policy {
            SpellCheckServiceEnabled {
                policy_options {mode: MANDATORY}
                SpellCheckServiceEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = BuildEndpointUrl(type);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  simple_url_loader->AttachStringForUpload(request_body, "application/json");

  auto it = spellcheck_loaders_.insert(
      spellcheck_loaders_.begin(),
      std::make_unique<TextCheckCallbackData>(std::move(simple_url_loader),
                                              std::move(callback), text));
  network::SimpleURLLoader* loader = it->get()->simple_url_loader.get();
  auto url_loader_factory = url_loader_factory_for_testing_
                                ? url_loader_factory_for_testing_
                                : context->GetDefaultStoragePartition()
                                      ->GetURLLoaderFactoryForBrowserProcess();
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(&SpellingServiceClient::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(it),
                     base::TimeTicks::Now()));
  return true;
}

bool SpellingServiceClient::IsAvailable(content::BrowserContext* context,
                                        ServiceType type) {
  const PrefService* pref = user_prefs::UserPrefs::Get(context);
  DCHECK(pref);
  // If prefs don't allow spell checking, if enhanced spell check is disabled,
  // or if the context is off the record, the spelling service should be
  // unavailable.
  if (!pref->GetBoolean(spellcheck::prefs::kSpellCheckEnable) ||
      !pref->GetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService) ||
      context->IsOffTheRecord())
    return false;

  // If the locale for spelling has not been set, the user has not decided to
  // use spellcheck so we don't do anything remote (suggest or spelling).
  std::string locale;
  const auto& dicts_list =
      pref->GetList(spellcheck::prefs::kSpellCheckDictionaries);
  if (!dicts_list.empty() && dicts_list[0].is_string()) {
    locale = dicts_list[0].GetString();
  }

  if (locale.empty())
    return false;

  // Finally, if all options are available, we only enable only SUGGEST
  // if SPELLCHECK is not available for our language because SPELLCHECK results
  // are a superset of SUGGEST results.
  for (const char* language : kValidLanguages) {
    if (!locale.compare(0, 2, language))
      return type == SPELLCHECK;
  }

  // Only SUGGEST is allowed.
  return type == SUGGEST;
}

void SpellingServiceClient::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory>
        url_loader_factory_for_testing) {
  url_loader_factory_for_testing_ = std::move(url_loader_factory_for_testing);
}

GURL SpellingServiceClient::BuildEndpointUrl(int type) {
    return GURL(base::StringPrintf(kSpellingServiceRestURL, type,
                                   google_apis::GetAPIKey().c_str()));
}

bool SpellingServiceClient::ParseResponse(
    const std::string& data,
    std::vector<SpellCheckResult>* results) {
  // Data is in the following format:
  //  * spellingCheckResponse: A wrapper object containing the response
  //    * mispellings: (optional Array<object>) A list of mistakes for the
  //      requested text, with the following format:
  //      * charStart: (number) The zero-based start of the misspelled region
  //      * charLength: (number) The length of the misspelled region
  //      * suggestions: (Array<object>) The suggestions for the misspelled
  //        text, with the following format:
  //        * suggestion: (string) the suggestion for the correct text
  //      * canAutoCorrect (optional boolean) Whether we can use the first
  //        suggestion for auto-correction
  //
  // Example response for "duck goes quisk":
  //  {
  //    "spellingCheckResponse": {
  //      "misspellings": [{
  //        "charStart": 10,
  //        "charLength": 5,
  //        "suggestions": [{
  //          "suggestion": "quack"
  //        }],
  //        "canAutoCorrect": false
  //      }]
  //    }
  //  }
  //
  // If the service is not available, the Spelling service returns JSON with an
  // error:
  //  {
  //    "error": {
  //      "code": 400,
  //      "message": "Bad Request",
  //      "data": [...]
  //    }
  //  }

  std::optional<base::Value::Dict> value =
      base::JSONReader::ReadDict(data, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value) {
    return false;
  }

  // Check for errors from spelling service.
    const base::Value* error = value->Find(kErrorPath);
    if (error) {
    return false;
    }

  // Retrieve the array of Misspelling objects. When the input text does not
  // have misspelled words, it returns an empty JSON. (In this case, its HTTP
  // status is 200.) We just return true for this case.
    const base::Value::List* misspellings =
        value->FindListByDottedPath(kMisspellingsRestPath);

    if (!misspellings) {
    return true;
    }

    for (const base::Value& misspelling : *misspellings) {
    // Retrieve the i-th misspelling region and put it to the given vector. When
    // the Spelling service sends two or more suggestions, we read only the
    // first one because SpellCheckResult can store only one suggestion.
    auto* misspelling_dict = misspelling.GetIfDict();
    if (!misspelling_dict) {
      return false;
    }

    std::optional<int> start = misspelling_dict->FindInt("charStart");
    std::optional<int> length = misspelling_dict->FindInt("charLength");
    const base::Value::List* suggestions =
        misspelling_dict->FindList("suggestions");
    if (!start || !length || !suggestions) {
      return false;
    }

    const auto* suggestion = suggestions->front().GetIfDict();
    if (!suggestion) {
      return false;
    }

    const std::string* replacement = suggestion->FindString("suggestion");
    if (!replacement) {
      return false;
    }
    SpellCheckResult result(SpellCheckResult::SPELLING, *start, *length,
                            base::UTF8ToUTF16(*replacement));
    results->push_back(result);
    }
  return true;
}

SpellingServiceClient::TextCheckCallbackData::TextCheckCallbackData(
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    TextCheckCompleteCallback callback,
    std::u16string text)
    : simple_url_loader(std::move(simple_url_loader)),
      callback(std::move(callback)),
      text(text) {}

SpellingServiceClient::TextCheckCallbackData::~TextCheckCallbackData() {}

void SpellingServiceClient::OnSimpleLoaderComplete(
    SpellCheckLoaderList::iterator it,
    base::TimeTicks request_start,
    std::unique_ptr<std::string> response_body) {
  UMA_HISTOGRAM_TIMES("SpellCheck.SpellingService.RequestDuration",
                      base::TimeTicks::Now() - request_start);

  TextCheckCompleteCallback callback = std::move(it->get()->callback);
  std::u16string text = it->get()->text;
  bool success = false;
  std::vector<SpellCheckResult> results;
  if (response_body)
    success = ParseResponse(*response_body, &results);

  int response_code = net::ERR_FAILED;
  auto* resp_info = it->get()->simple_url_loader->ResponseInfo();
  if (resp_info && resp_info->headers) {
    response_code = resp_info->headers->response_code();
  }

  ServiceRequestResultType result_type =
      ServiceRequestResultType::kRequestFailure;
  if (success) {
    result_type = results.empty()
                      ? ServiceRequestResultType::kSuccessEmpty
                      : ServiceRequestResultType::kSuccessWithSuggestions;
  }

  base::UmaHistogramSparse("SpellCheck.SpellingService.RequestHttpResponseCode",
                           response_code);
  UMA_HISTOGRAM_ENUMERATION("SpellCheck.SpellingService.RequestResultType",
                            result_type);

  spellcheck_loaders_.erase(it);
  std::move(callback).Run(success, text, results);
}
