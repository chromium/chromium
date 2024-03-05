// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_loader.h"

#include <string_view>
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

constexpr std::string_view kQueryKey = "query";
constexpr std::string_view kRawQueryKey = "rawQuery";
constexpr std::string_view kClientTypeKey = "clientType";
constexpr std::string_view kClientIdKey = "clientId";
constexpr std::string_view kClientType = "QUICK_ANSWERS_CROS";
constexpr std::string_view kLanguageCodeKey = "languageCode";
constexpr std::string_view kLanguageContextKey = "languageContext";
constexpr std::string_view kRequestContextKey = "requestContext";

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
      std::make_unique<SearchResponseParser>(std::move(complete_callback));
  search_response_parser_->ProcessResponse(std::move(response_body));
}

}  // namespace quick_answers
