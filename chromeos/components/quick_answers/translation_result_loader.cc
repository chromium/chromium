// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_result_loader.h"

#include <string_view>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
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

constexpr std::string_view kQueryKey = "q";
constexpr std::string_view kSourceLanguageKey = "source";
constexpr std::string_view kTargetLanguageKey = "target";

std::string BuildTranslationRequestBody(const IntentInfo& intent_info) {
  Value::Dict payload;

  Value::List query;
  query.Append(intent_info.intent_text);
  payload.Set(kQueryKey, std::move(query));

  payload.Set(kSourceLanguageKey, intent_info.source_language);
  payload.Set(kTargetLanguageKey, intent_info.device_language);

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
  if (translation_response_parser_) {
    DCHECK(false) << "translation_response_parser_ must be nullptr";
    std::move(complete_callback).Run(nullptr);
    return;
  }

  translation_response_parser_ =
      std::make_unique<TranslationResponseParser>(base::BindOnce(
          &TranslationResultLoader::ProcessParsedResponse,
          weak_ptr_factory_.GetWeakPtr(), preprocessed_output.intent_info,
          std::move(complete_callback)));
  translation_response_parser_->ProcessResponse(std::move(response_body));
}

void TranslationResultLoader::ProcessParsedResponse(
    IntentInfo intent_info,
    ResponseParserCallback complete_callback,
    std::unique_ptr<TranslationResult> translation_result) {
  translation_response_parser_.reset();

  if (!translation_result || translation_result->translated_text.empty()) {
    std::move(complete_callback).Run(nullptr);
    return;
  }

  translation_result->text_to_translate = intent_info.intent_text;
  translation_result->source_locale = intent_info.source_language;
  translation_result->target_locale = intent_info.device_language;

  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->result_type = ResultType::kTranslationResult;
  quick_answer->title.push_back(std::make_unique<QuickAnswerText>(
      BuildTranslationTitleText(intent_info)));
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>(
          translation_result->translated_text));

  std::unique_ptr<QuickAnswersSession> session =
      std::make_unique<QuickAnswersSession>();
  session->structured_result = std::make_unique<StructuredResult>();
  session->structured_result->translation_result =
      std::move(translation_result);
  session->quick_answer = std::move(quick_answer);

  std::move(complete_callback).Run(std::move(session));
}

}  // namespace quick_answers
