// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_loader.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace quick_answers {
namespace {

using base::Value;

// The JSON we generate looks like this:
// {
//   "query": {
//     "raw_query": "23 cm"
//   }
//   "client_id": {
//     "client_type": "EXPERIMENTAL"
//   }
//   "request_context": {
//     "language_context": {
//        "language_code": "en-US"
//      }
//   }
// }
//
// Which is:
// DICT
//   "query": DICT
//     "raw_query": STRING
//   "client_id": DICT
//     "client_type": STRING
//   "request_context": DICT
//     "language_context": DICT
//       "language_code": STRING

constexpr base::StringPiece kQueryKey = "query";
constexpr base::StringPiece kRawQueryKey = "rawQuery";
constexpr base::StringPiece kClientTypeKey = "clientType";
constexpr base::StringPiece kClientIdKey = "clientId";
constexpr base::StringPiece kClientType = "QUICK_ANSWERS_CROS";
constexpr base::StringPiece kLanguageCodeKey = "languageCode";
constexpr base::StringPiece kLanguageContextKey = "languageContext";
constexpr base::StringPiece kRequestContextKey = "requestContext";

std::string BuildSearchRequestPayload(const std::string& selected_text,
                                      const std::string& device_language) {
  Value::Dict payload;

  Value::Dict query;
  query.Set(kRawQueryKey, selected_text);
  payload.Set(kQueryKey, std::move(query));

  // TODO(llin): Change the client type.
  Value::Dict client_id;
  client_id.Set(kClientTypeKey, kClientType);
  payload.Set(kClientIdKey, std::move(client_id));

  Value::Dict request_context;
  Value::Dict language_context;
  language_context.Set(kLanguageCodeKey, device_language);
  request_context.Set(kLanguageContextKey, std::move(language_context));
  payload.Set(kRequestContextKey, std::move(request_context));

  std::string request_payload_str;
  base::JSONWriter::Write(payload, &request_payload_str);

  return request_payload_str;
}

}  // namespace

SearchResultLoader::SearchResultLoader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ResultLoaderDelegate* delegate)
    : ResultLoader(url_loader_factory, delegate) {}

SearchResultLoader::~SearchResultLoader() = default;

void SearchResultLoader::BuildRequest(
    const PreprocessedOutput& preprocessed_output,
    BuildRequestCallback callback) const {
  GURL url = GURL(chromeos::assistant::kKnowledgeApiEndpoint);

  // Add encoded request payload.
  url = net::AppendOrReplaceQueryParameter(
      url, chromeos::assistant::kPayloadParamName,
      BuildSearchRequestPayload(
          preprocessed_output.query,
          preprocessed_output.intent_info.device_language));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  std::move(callback).Run(std::move(resource_request), std::string());
}

void SearchResultLoader::ProcessResponse(
    const PreprocessedOutput& preprocessed_output,
    std::unique_ptr<std::string> response_body,
    ResponseParserCallback complete_callback) {
  search_response_parser_ =
      std::make_unique<SearchResponseParser>(base::BindOnce(
          &SearchResultLoader::OnSearchResponseParsed,
          weak_ptr_factory_.GetWeakPtr(), std::move(complete_callback)));
  search_response_parser_->ProcessResponse(std::move(response_body));
}

void SearchResultLoader::OnSearchResponseParsed(
    ResponseParserCallback complete_callback,
    std::unique_ptr<QuickAnswer> quick_answer) {
  // If no `QuickAnswer` is returned, e.g. parse failure, return nullptr instead
  // of an empty `QuickAnswersSession`. `SearchResultLoaderTest.EmptyResponse`
  // expects this behavior. For longer term, migrate to an empty `quick_answer`
  // field in `QuickAnswersSession` as `QuickAnswersSession` will hold more
  // information, e.g. intent.
  if (!quick_answer) {
    std::move(complete_callback).Run(nullptr);
    return;
  }

  // TODO(b/278929409) Fill structured_result field.
  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      std::make_unique<QuickAnswersSession>();
  quick_answers_session->quick_answer = std::move(quick_answer);
  std::move(complete_callback).Run(std::move(quick_answers_session));
}

}  // namespace quick_answers
