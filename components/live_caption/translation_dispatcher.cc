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
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/soda/constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
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
                                           OnTranslateEventCallback callback) {
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
  url_loader_->SetAllowHttpErrorResults(true);

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
    OnTranslateEventCallback callback,
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    LOG(ERROR) << "Error parsing response: reponse body null";
    return;
  }

  // Parse the response in a utility process.
  data_decoder_.ParseJson(
      *response_body,
      base::BindOnce(&TranslationDispatcher::OnResponseJsonParsed,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void TranslationDispatcher::OnResponseJsonParsed(
    OnTranslateEventCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  std::string error = [&]() -> std::string {
    if (!result.has_value()) {
      return "Error parsing response: value null";
    }

    if (!result.value().is_dict()) {
      return "Error parsing response: result value is not a dictionary";
    }

    const base::Value::Dict* data_dict =
        result.value().GetDict().FindDict(kDataKey);
    if (!data_dict) {
      return "Error parsing response: dictionary not found";
    }

    const base::Value::List* translations_list =
        data_dict->FindList(kTranslationsKey);
    if (!translations_list || translations_list->empty()) {
      return "Error parsing response: translations not found";
    }

    const base::Value::Dict* translated_text =
        (*translations_list)[0].GetIfDict();
    if (!translated_text) {
      return "Error parsing response: translated text not found";
    }

    if (const std::string* value =
            translated_text->FindString(kTranslatedTextKey)) {
      std::move(callback).Run(*value);
    }
    return std::string();
  }();
  if (!error.empty()) {
    LOG(ERROR) << std::move(error);
  }
}

}  // namespace captions
