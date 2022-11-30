// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_result_loader.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace quick_answers {
namespace {

using base::Value;

// The JSON we generate looks like this:
// {
//  "q": [
//    "test input"
//  ],
//  "source": "en",
//  "target": "zh"
// }

constexpr char kTranslationAPIUrl[] =
    "https://translation.googleapis.com/language/translate/v2";
constexpr char kApiKeyName[] = "key";

constexpr base::StringPiece kQueryKey = "q";
constexpr base::StringPiece kSourceLanguageKey = "source";
constexpr base::StringPiece kTargetLanguageKey = "target";

std::string BuildTranslationRequestBody(const IntentInfo& intent_info) {
  Value payload(Value::Type::DICTIONARY);

  Value query(Value::Type::LIST);
  query.Append(intent_info.intent_text);
  payload.SetKey(kQueryKey, std::move(query));

  payload.SetKey(kSourceLanguageKey, Value(intent_info.source_language));
  payload.SetKey(kTargetLanguageKey, Value(intent_info.device_language));

  std::string request_payload_str;
  base::JSONWriter::Write(payload, &request_payload_str);

  return request_payload_str;
}

}  // namespace

TranslationResultLoader::TranslationResultLoader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ResultLoaderDelegate* delegate)
    : ResultLoader(url_loader_factory, delegate) {}

TranslationResultLoader::~TranslationResultLoader() = default;

void TranslationResultLoader::BuildRequest(
    const PreprocessedOutput& preprocessed_output,
    BuildRequestCallback callback) const {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = net::AppendQueryParameter(
      GURL(kTranslationAPIUrl), kApiKeyName, google_apis::GetAPIKey());
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/json");

  std::move(callback).Run(
      std::move(resource_request),
      BuildTranslationRequestBody(preprocessed_output.intent_info));
}

void TranslationResultLoader::ProcessResponse(
    const PreprocessedOutput& preprocessed_output,
    std::unique_ptr<std::string> response_body,
    ResponseParserCallback complete_callback) {
  translation_response_parser_ =
      std::make_unique<TranslationResponseParser>(std::move(complete_callback));
  translation_response_parser_->ProcessResponse(
      std::move(response_body),
      BuildTranslationTitleText(preprocessed_output.intent_info));
}

}  // namespace quick_answers
