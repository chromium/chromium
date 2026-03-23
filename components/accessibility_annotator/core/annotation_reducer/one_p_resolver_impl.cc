// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/one_p_resolver_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/annotation_reducer/one_p_service.pb.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/annotation_reducer_one_p_resolver.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace accessibility_annotator {

OnePResolverImpl::OnePResolverImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    optimization_guide::RemoteModelExecutor* remote_model_executor)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      remote_model_executor_(remote_model_executor) {}

OnePResolverImpl::~OnePResolverImpl() = default;

void OnePResolverImpl::Query(std::u16string query, QueryCallback callback) {
  // Explicitly cancels any in-flight request and invokes its callback with an
  // empty result set. This enforces the contract that only one request can
  // be active at a time.
  if (in_flight_query_callback_) {
    std::move(in_flight_query_callback_).Run({});
  }
  in_flight_query_callback_ = std::move(callback);

  // Cancel any asynchronous operations (token fetch or URL loading) that were
  // tied to the previous request.
  weak_ptr_factory_.InvalidateWeakPtrs();
  simple_url_loader_.reset();

  // Helper to post an empty callback securely, avoiding re-entrancy issues
  // with synchronous callers.
  auto post_empty = [&]() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(in_flight_query_callback_),
                                  std::vector<MemorySearchResult>()));
  };

  // Pre-flight checks. Validate feature state, user sign-in status, and the
  // configured endpoint URL.
  if (!base::FeatureList::IsEnabled(
          kAccessibilityAnnotationReducerOnePResolver)) {
    post_empty();
    return;
  }

  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    post_empty();
    return;
  }

  GURL url(kAccessibilityAnnotatorOnePServiceUrl.Get());
  if (!url.is_valid()) {
    post_empty();
    return;
  }

  // Request an OAuth access token to authenticate the service request.
  access_token_fetcher_.reset();
  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      signin::OAuthConsumerId::kAccessibilityAnnotator,
      base::BindOnce(&OnePResolverImpl::OnAccessTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(query),
                     std::move(url)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void OnePResolverImpl::OnAccessTokenFetched(
    std::u16string query,
    GURL url,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // Helper to immediately execute the callback with an empty result set.
  auto run_empty = [&]() {
    if (in_flight_query_callback_) {
      std::move(in_flight_query_callback_).Run({});
    }
  };

  // Clean up the fetcher now that the token request is complete.
  // This is safe to do inside the callback.
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    run_empty();
    return;
  }

  // Construct the HTTP request with the newly acquired access token.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  resource_request->method = "POST";
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();

  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + access_token_info.token);

  // TODO(b:494582740): Add real traffic annotation later.
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), MISSING_TRAFFIC_ANNOTATION);

  // Serialize the query into the protobuf request body.
  accessibility_annotator::OnePAnnotationsRequest request_proto;
  request_proto.set_query(base::UTF16ToUTF8(query));
  std::string request_body;
  request_proto.SerializeToString(&request_body);

  simple_url_loader_->AttachStringForUpload(std::move(request_body),
                                            "application/x-protobuf");

  // Execute the network request to fetch annotations.
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&OnePResolverImpl::OnUrlLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), base::UTF16ToUTF8(query)));
}

void OnePResolverImpl::OnUrlLoadComplete(
    std::string query_string,
    std::optional<std::string> response_body) {
  // Helper to immediately execute the callback with an empty result set.
  auto run_empty = [&]() {
    if (in_flight_query_callback_) {
      std::move(in_flight_query_callback_).Run({});
    }
  };

  // Validate the network response and clean up the loader.
  if (!simple_url_loader_ || simple_url_loader_->NetError() != net::OK ||
      !response_body) {
    simple_url_loader_.reset();
    run_empty();
    return;
  }
  simple_url_loader_.reset();

  // Parse the protobuf response.
  accessibility_annotator::OnePAnnotationsResponse response_proto;
  if (!response_proto.ParseFromString(*response_body)) {
    run_empty();
    return;
  }

  // Extract the embedded JSON payload from the response proto.
  // The response is wrapped in a JSON string containing the 'context'.
  std::optional<base::Value> root_value =
      base::JSONReader::Read(response_proto.response(), base::JSON_PARSE_RFC);
  if (!root_value || !root_value->is_dict()) {
    run_empty();
    return;
  }

  const std::string* context_str = root_value->GetDict().FindString("context");
  if (!context_str) {
    run_empty();
    return;
  }

  if (!remote_model_executor_) {
    run_empty();
    return;
  }

  // Delegate the extracted context and original query to the model executor.
  optimization_guide::proto::AnnotationReducerOnePResolverRequest request;
  request.set_query(std::move(query_string));
  request.set_context(*context_str);

  remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::
          kAnnotationReducerOnePResolver,
      request, optimization_guide::ModelExecutionOptions(),
      base::BindOnce(&OnePResolverImpl::OnModelExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnePResolverImpl::OnModelExecutionComplete(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  // Helper to immediately execute the callback with an empty result set.
  auto run_empty = [&]() {
    if (in_flight_query_callback_) {
      std::move(in_flight_query_callback_).Run({});
    }
  };

  if (!result.response.has_value()) {
    run_empty();
    return;
  }

  // Unpack the model execution response.
  const optimization_guide::proto::Any& response_any = result.response.value();
  std::optional<
      optimization_guide::proto::AnnotationReducerOnePResolverResponse>
      response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::AnnotationReducerOnePResolverResponse>(
          response_any);

  if (!response) {
    run_empty();
    return;
  }

  std::string_view response_string = response->answer();

  // Strip markdown code blocks if present. Models may wrap JSON in
  // ```json...```.
  if (base::StartsWith(response_string, "```json",
                       base::CompareCase::SENSITIVE)) {
    response_string.remove_prefix(7);
  } else if (base::StartsWith(response_string, "```",
                              base::CompareCase::SENSITIVE)) {
    response_string.remove_prefix(3);
  }

  if (base::EndsWith(response_string, "```", base::CompareCase::SENSITIVE)) {
    response_string.remove_suffix(3);
  }

  response_string = base::TrimWhitespaceASCII(response_string, base::TRIM_ALL);

  // Parse the sanitized model response string into JSON.
  std::optional<base::Value> value =
      base::JSONReader::Read(response_string, base::JSON_PARSE_RFC);
  if (!value || !value->is_list()) {
    run_empty();
    return;
  }

  // Extract each dictionary item into a MemorySearchResult.
  std::vector<MemorySearchResult> results;
  for (const base::Value& item : value->GetList()) {
    const base::DictValue* dict = item.GetIfDict();
    if (!dict) {
      continue;
    }

    const std::string* value_str = dict->FindString("value");
    const std::string* title_str = dict->FindString("title");
    const std::string* description_str = dict->FindString("description");

    // We require all three core fields to consider the result valid.
    if (!value_str || !title_str || !description_str) {
      continue;
    }

    MemorySearchResult search_result;
    search_result.value = base::UTF8ToUTF16(*value_str);
    search_result.title = base::UTF8ToUTF16(*title_str);
    search_result.description = base::UTF8ToUTF16(*description_str);

    if (std::optional<double> score = dict->FindDouble("ranking_score")) {
      search_result.ranking_score = *score;
    } else if (const std::string* score_str =
                   dict->FindString("ranking_score")) {
      base::StringToDouble(*score_str, &search_result.ranking_score);
    }

    results.push_back(std::move(search_result));
  }

  if (in_flight_query_callback_) {
    std::move(in_flight_query_callback_).Run(std::move(results));
  }
}

}  // namespace accessibility_annotator
