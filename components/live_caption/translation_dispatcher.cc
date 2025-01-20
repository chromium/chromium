// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/translation_dispatcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/live_caption/translation_util.h"
#include "components/soda/constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace captions {

// Request constants.
const size_t kMaxMessageSize = 1024 * 1024;  // 1MB
constexpr char kTranslateBodyRequestTemplate[] =
    "{"
    "\"q\":\"%s\","
    "\"source\":\"%s\","
    "\"target\":\"%s\","
    "\"format\":\"text\""
    "}";
constexpr char kTranslateUrl[] =
    "https://translation.googleapis.com/language/translate/v2?key=%s";
constexpr char kUploadContentType[] = "application/json";
constexpr char kHttpErrorMessageTemplate[] =
    "Failed to recieve response, got errror: %s";

// Response constants.
constexpr char kDataKey[] = "data";
constexpr char kTranslationsKey[] = "translations";
constexpr char kTranslatedTextKey[] = "translatedText";

TranslationDispatcher::TranslationDispatcher(
    std::string api_key,
    content::BrowserContext* browser_context)
    : api_key_(api_key), browser_context_(browser_context) {}

TranslationDispatcher::~TranslationDispatcher() = default;

void TranslationDispatcher::GetTranslation(const std::string& result,
                                           std::string source_language,
                                           std::string target_language,
                                           TranslateEventCallback callback) {
  if (!url_loader_factory_.is_bound() || !url_loader_factory_.is_connected()) {
    ResetURLLoaderFactory();
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL(base::StringPrintf(kTranslateUrl, api_key_.c_str()));

  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("cloud_speech_recognition",
                                          R"(
        semantics {
          sender: "Live Translate"
          description:
            "Chrome provides the ability to translate captions generated from "
            "output audio by using the Google Cloud Translate web service. "
            "Captions are sent to Google's servers and translated captions are "
            "returned."
          trigger:
            "Triggered in direct response to a user playing a media with audio "
            "with the Live Translate feature enabled."
          data: "Captions to be translated by Google's Cloud Translate web "
            "service."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "evanliu@google.com"
            }
          }
          user_data {
            type: OTHER
          }
          last_reviewed: "2024-10-11"
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via the caption settings "
            "page."
          chrome_policy {
            TranslateEnabled {
                TranslateEnabled: false
            }
          }
        })");
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(
      base::StringPrintf(kTranslateBodyRequestTemplate, result.c_str(),
                         source_language.c_str(), target_language.c_str()),
      kUploadContentType);

  // Unretained is safe because |this| owns |url_loader_|.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&TranslationDispatcher::OnURLLoadComplete,
                     base::Unretained(this), std::move(callback)),
      kMaxMessageSize);

  base::UmaHistogramSparse("Accessibility.LiveTranslate.TargetLanguage",
                           base::HashMetricName(target_language));
  base::UmaHistogramSparse("Accessibility.LiveTranslate.SourceLanguage",
                           base::HashMetricName(source_language));
}

void TranslationDispatcher::ResetURLLoaderFactory() {
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_trusted = false;
  params->automatically_assign_isolation_info = true;
  network::mojom::NetworkContext* network_context =
      browser_context_->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
}

void TranslationDispatcher::OnURLLoadComplete(
    TranslateEventCallback callback,
    std::optional<std::string> response_body) {
  // Check that the request succeeded. First with Network Errors...
  if (static_cast<net::Error>(url_loader_->NetError()) != net::Error::OK) {
    EmitError(
        std::move(callback),
        base::StringPrintf(kHttpErrorMessageTemplate,
                           net::ErrorToShortString(url_loader_->NetError())));
    return;
  }

  // and then check HTTP errors.
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers &&
      !network::IsSuccessfulStatus(
          url_loader_->ResponseInfo()->headers->response_code())) {
    EmitError(std::move(callback),
              base::StringPrintf(
                  kHttpErrorMessageTemplate,
                  base::NumberToString(
                      url_loader_->ResponseInfo()->headers->response_code())));
    return;
  }

  // Somehow the request succeeded but the body is empty.
  if (!response_body.has_value()) {
    EmitError(std::move(callback),
              "Error parsing response: Translation dispatcher recieved a 2XX "
              "response, but the body was empty");
    return;
  }

  // Parse the response in a utility process.
  data_decoder_.ParseJson(
      *response_body,
      base::BindOnce(&TranslationDispatcher::OnResponseJsonParsed,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void TranslationDispatcher::EmitError(TranslateEventCallback callback,
                                      const std::string& message) const {
  std::move(callback).Run(base::unexpected<std::string>(message));
}

void TranslationDispatcher::OnResponseJsonParsed(
    TranslateEventCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    EmitError(std::move(callback), "Error parsing response: value null");
    return;
  }

  if (!result.value().is_dict()) {
    EmitError(std::move(callback),
              "Error parsing response: result value is not a dictionary");
    return;
  }

  const base::Value::Dict* data_dict =
      result.value().GetDict().FindDict(kDataKey);
  if (!data_dict) {
    EmitError(std::move(callback),
              "Error parsing response: dictionary not found");
    return;
  }

  const base::Value::List* translations_list =
      data_dict->FindList(kTranslationsKey);
  if (!translations_list || translations_list->empty()) {
    EmitError(std::move(callback),
              "Error parsing response: translations not found");
    return;
  }

  const base::Value::Dict* translated_text =
      (*translations_list)[0].GetIfDict();
  if (!translated_text) {
    EmitError(std::move(callback),
              "Error parsing response: translated list entry not found");
    return;
  }

  const std::string* value = translated_text->FindString(kTranslatedTextKey);
  if (!value) {
    EmitError(std::move(callback),
              "Error parsing response: translated text not found");
    return;
  }

  std::move(callback).Run(TranslateEvent(*value));
}

}  // namespace captions
