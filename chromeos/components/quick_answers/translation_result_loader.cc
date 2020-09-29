// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_result_loader.h"

#include <utility>

#include "ash/public/cpp/quick_answers/controller/quick_answers_browser_client.h"
#include "base/json/json_writer.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace chromeos {
namespace quick_answers {
namespace {

using base::Value;
using network::mojom::URLLoaderFactory;

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
constexpr char kAuthorizationHeaderFormat[] = "Bearer ";

constexpr base::StringPiece kQueryKey = "q";
constexpr base::StringPiece kSourceLanguageKey = "source";
constexpr base::StringPiece kTargetLanguageKey = "target";

std::string BuildTranslationRequestBody(const IntentInfo& intent_info) {
  Value payload(Value::Type::DICTIONARY);

  Value query(Value::Type::LIST);
  query.Append(intent_info.intent_text);
  payload.SetKey(kQueryKey, std::move(query));

  payload.SetKey(kSourceLanguageKey, Value(intent_info.source_language));
  payload.SetKey(kTargetLanguageKey, Value(intent_info.target_language));

  std::string request_payload_str;
  base::JSONWriter::Write(payload, &request_payload_str);

  return request_payload_str;
}

}  // namespace

TranslationResultLoader::TranslationResultLoader(
    URLLoaderFactory* url_loader_factory,
    ResultLoaderDelegate* delegate)
    : ResultLoader(url_loader_factory, delegate) {}

TranslationResultLoader::~TranslationResultLoader() = default;

void TranslationResultLoader::BuildRequest(
    const PreprocessedOutput& preprocessed_output,
    BuildRequestCallback callback) const {
  ash::QuickAnswersBrowserClient::Get()->RequestAccessToken(base::BindOnce(
      &TranslationResultLoader::OnRequestAccessTokenComplete,
      base::Unretained(this), preprocessed_output, std::move(callback)));
}

void TranslationResultLoader::ProcessResponse(
    std::unique_ptr<std::string> response_body,
    ResponseParserCallback complete_callback) {
  translation_response_parser_ =
      std::make_unique<TranslationResponseParser>(std::move(complete_callback));
  translation_response_parser_->ProcessResponse(std::move(response_body));
}

void TranslationResultLoader::OnRequestAccessTokenComplete(
    const PreprocessedOutput& preprocessed_output,
    BuildRequestCallback callback,
    const std::string& access_token) const {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kTranslationAPIUrl);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      kAuthorizationHeaderFormat + access_token);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");

  auto body = BuildTranslationRequestBody(preprocessed_output.intent_info);
  resource_request->request_body = new network::ResourceRequestBody();
  resource_request->request_body->AppendBytes(body.c_str(), body.length());

  std::move(callback).Run(std::move(resource_request));
}

}  // namespace quick_answers
}  // namespace chromeos
