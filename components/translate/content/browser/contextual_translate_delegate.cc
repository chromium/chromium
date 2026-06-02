// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/contextual_translate_delegate.h"

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
constexpr char kOnePlatformEndpoint[] =
    "https://translate-pa.googleapis.com/v1/translateHtml";
constexpr char kOnePlatformEuEndpoint[] =
    "https://translate-pa.eu.rep.googleapis.com/v1/translateHtml";
constexpr char kOnePlatformUsEndpoint[] =
    "https://translate-pa.us.rep.googleapis.com/v1/translateHtml";
constexpr char kClientName[] = "chrome";
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;  // 1MB limit
}  // namespace

GURL ContextualTranslateDelegate::BuildEndpointUrl(PrefService* prefs) const {
  const char* base_url = kOnePlatformEndpoint;
  if (prefs) {
    switch (static_cast<translate::DataRegion>(
        prefs->GetInteger(translate::prefs::kTranslateDataRegionSetting))) {
      case translate::DataRegion::kUnitedStates:
        base_url = kOnePlatformUsEndpoint;
        break;
      case translate::DataRegion::kEurope:
        base_url = kOnePlatformEuEndpoint;
        break;
      case translate::DataRegion::kNoPreference:
        break;
    }
  }
  return GURL(base_url);
}

std::string ContextualTranslateDelegate::CreatePostData(
    const PartialTranslateRequest& request) const {
  base::Value query_set(base::Value::Type::LIST);
  base::Value queries(base::Value::Type::LIST);
  queries.GetList().Append(base::UTF16ToUTF8(request.selection_text));
  query_set.GetList().Append(std::move(queries));
  if (request.source_language.has_value() &&
      !request.source_language->empty() && !request.apply_lang_hint) {
    query_set.GetList().Append(*request.source_language);
  } else {
    query_set.GetList().Append(base::Value());
  }
  query_set.GetList().Append(request.target_language);
  if (request.apply_lang_hint) {
    query_set.GetList().Append(
        translate::TranslateDownloadManager::GetInstance()
            ->application_locale());
  }

  base::Value request_list(base::Value::Type::LIST);
  request_list.GetList().Append(std::move(query_set));
  request_list.GetList().Append(kClientName);

  std::optional<std::string> result = base::WriteJson(request_list);
  CHECK(result.has_value());
  return std::move(result).value();
}

bool ContextualTranslateDelegate::CancelPendingRequest() {
  if (!pending_callback_) {
    url_loader_.reset();
    weak_ptr_factory_.InvalidateWeakPtrs();
    return true;
  }

  PartialTranslateResponse response;
  response.status = PartialTranslateStatus::kError;
  auto cb = std::move(pending_callback_);
  url_loader_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  std::move(cb).Run(response);
  return !!weak_this;
}

ContextualTranslateDelegate::ContextualTranslateDelegate(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

ContextualTranslateDelegate::~ContextualTranslateDelegate() = default;

void ContextualTranslateDelegate::StartPartialTranslate(
    const PartialTranslateRequest& request,
    PrefService* prefs,
    Callback callback) {
  // Immediately cancel any request that's in flight.
  if (!CancelPendingRequest()) {
    return;
  }
  pending_callback_ = std::move(callback);

  GURL url = BuildEndpointUrl(prefs);
  std::string post_data = CreatePostData(request);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader("X-Goog-Api-Key",
                                      google_apis::GetPartialTranslateAPIKey());
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("partial_translate_one_platform", R"(
        semantics {
          sender: "Partial Translate"
          description:
            "Sends user-selected text on a webpage to Google Translate "
            "to provide an on-demand translation. This helps users "
            "understand foreign language text snippets."
          trigger:
            "User right-clicks on selected text and chooses the 'Translate' "
            "option from the context menu."
          data:
            "Selected page content from the current tab."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: WEB_CONTENT
          }
          internal {
            contacts {
              email: "chrome-translate@google.com"
            }
          }
          last_reviewed: "2026-02-12"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled by turning off the "
            "'Offer to translate pages that aren't in a language you read' "
            "option in Chrome settings -> Languages."
          chrome_policy {
            TranslateEnabled {
              TranslateEnabled: false
            }
          }
        })");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(post_data, "application/json+protobuf");
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ContextualTranslateDelegate::OnUrlLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), request.target_language,
                     request.source_language),
      kMaxResponseSizeInBytes);
}

void ContextualTranslateDelegate::OnUrlLoadComplete(
    std::string target_language,
    std::optional<std::string> source_language,
    std::optional<std::string> response_body) {
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  url_loader_.reset();
  base::UmaHistogramSparse(kTranslatePartialTranslationHttpResponseCode,
                           response_code);

  if ((!response_body.has_value() || response_code != 200) &&
      pending_callback_) {
    PartialTranslateResponse response;
    response.status = PartialTranslateStatus::kError;
    std::move(pending_callback_).Run(response);
    return;
  }

  // Parse JSON
  data_decoder_.ParseJson(
      *response_body,
      base::BindOnce(&ContextualTranslateDelegate::OnJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(target_language),
                     std::move(source_language)));
}

void ContextualTranslateDelegate::OnJsonParsed(
    std::string target_language,
    std::optional<std::string> source_language,
    data_decoder::DataDecoder::ValueOrError result) {
  PartialTranslateResponse response;
  response.status = PartialTranslateStatus::kError;

  // The translateHtml endpoint using JSPB returns a JSON array.
  // Field 1 (index 0): translations (array of strings)
  // Field 2 (index 1): detected_languages (array of strings)
  if (result.has_value() && result->is_list()) {
    const auto& list = result->GetList();
    if (!list.empty() && list[0].is_list() && !list[0].GetList().empty()) {
      const auto& translations = list[0].GetList();
      if (translations[0].is_string()) {
        response.translated_text =
            base::UTF8ToUTF16(translations[0].GetString());
        response.status = PartialTranslateStatus::kSuccess;
      }
    }
    if (list.size() > 1 && list[1].is_list() && !list[1].GetList().empty()) {
      const auto& detected_languages = list[1].GetList();
      if (detected_languages[0].is_string()) {
        response.source_language = detected_languages[0].GetString();
      }
    }
  }

  if (response.source_language.empty() && source_language.has_value()) {
    response.source_language = *source_language;
  }

  response.target_language = target_language;
  if (pending_callback_) {
    std::move(pending_callback_).Run(response);
  }
}
