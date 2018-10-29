// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "components/ntp_snippets/contextual/contextual_suggestions_fetch.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/ntp_snippets/contextual/contextual_suggestion.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"
#include "components/ntp_snippets/contextual/proto/chrome_search_api_request_context.pb.h"
#include "components/ntp_snippets/contextual/proto/get_pivots_request.pb.h"
#include "components/ntp_snippets/contextual/proto/get_pivots_response.pb.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

using base::Base64Encode;

namespace contextual_suggestions {

namespace {

static constexpr char kFetchEndpointUrlKey[] =
    "contextual-suggestions-fetch-endpoint";
static constexpr char kDefaultFetchEndpointUrl[] = "https://www.google.com";
static constexpr char kFetchEndpointServicePath[] =
    "/httpservice/web/ExploreService/GetPivots/";

static constexpr int kNumberOfSuggestionsToFetch = 10;
static constexpr int kMinNumberOfClusters = 1;
static constexpr int kMaxNumberOfClusters = 5;

// Wrapper for net::UnescapeForHTML that also handles utf8<->16 conversion.
std::string Unescape(const std::string& encoded_text) {
  return base::UTF16ToUTF8(
      net::UnescapeForHTML(base::UTF8ToUTF16(encoded_text)));
}

// Converts |item| to a ContextualSuggestion, un-escaping any HTML entities
// encountered.
ContextualSuggestion ItemToSuggestion(const PivotItem& item) {
  PivotDocument document = item.document();

  std::string favicon_url;
  if (document.favicon_image().source_data().has_raster()) {
    favicon_url =
        document.favicon_image().source_data().raster().url().raw_url();
  }

  return SuggestionBuilder(GURL(document.url().raw_url()))
      .Title(Unescape(document.title()))
      .Snippet(Unescape(document.summary()))
      .PublisherName(Unescape(document.site_name()))
      .ImageId(document.image().id().encrypted_docid())
      .FaviconImageId(document.favicon_image().id().encrypted_docid())
      .FaviconImageUrl(favicon_url)
      .Build();
}

Cluster PivotToCluster(const PivotCluster& pivot) {
  ClusterBuilder cluster_builder(pivot.label().label());
  for (PivotItem item : pivot.item()) {
    cluster_builder.AddSuggestion(ItemToSuggestion(item));
  }

  return cluster_builder.Build();
}

PeekConditions GetPeekConditionsFromResponse(
    const GetPivotsResponse& response_proto) {
  AutoPeekConditions proto_conditions =
      response_proto.pivots().auto_peek_conditions();
  PeekConditions peek_conditions;

  peek_conditions.confidence = proto_conditions.confidence();
  peek_conditions.page_scroll_percentage =
      proto_conditions.page_scroll_percentage();
  peek_conditions.minimum_seconds_on_page =
      proto_conditions.minimum_seconds_on_page();
  peek_conditions.maximum_number_of_peeks =
      proto_conditions.maximum_number_of_peeks();
  return peek_conditions;
}

std::vector<Cluster> GetClustersFromResponse(
    const GetPivotsResponse& response_proto) {
  std::vector<Cluster> clusters;
  Pivots pivots = response_proto.pivots();

  if (pivots.item_size() > 0) {
    // If the first item is a cluster, we can assume they all are.
    if (pivots.item()[0].has_cluster()) {
      clusters.reserve(response_proto.pivots().item_size());
      for (auto item : response_proto.pivots().item()) {
        if (item.has_cluster()) {
          PivotCluster pivot_cluster = item.cluster();
          clusters.emplace_back(PivotToCluster(pivot_cluster));
        }
      }
    } else if (pivots.item()[0].has_document()) {
      Cluster single_cluster;
      for (auto item : pivots.item()) {
        single_cluster.suggestions.emplace_back(ItemToSuggestion(item));
      }
      clusters.emplace_back(std::move(single_cluster));
    }
  }

  return clusters;
}

std::string GetPeekTextFromResponse(const GetPivotsResponse& response_proto) {
  return response_proto.pivots().peek_text().text();
}

const std::string SerializedPivotsRequest(const std::string& url,
                                          const std::string& bcp_language) {
  GetPivotsRequest pivot_request;

  SearchApiRequestContext* search_context = pivot_request.mutable_context();
  search_context->mutable_localization_context()->set_language_code(
      bcp_language);

  GetPivotsQuery* query = pivot_request.mutable_query();
  query->add_context()->set_url(url);

  PivotDocumentParams* params = query->mutable_document_params();
  params->set_enabled(true);
  params->set_num(kNumberOfSuggestionsToFetch);
  params->set_enable_images(true);
  params->set_image_aspect_ratio(PivotDocumentParams::SQUARE);

  PivotClusteringParams* cluster_params = query->mutable_clustering_params();
  cluster_params->set_enabled(true);
  cluster_params->set_min(kMinNumberOfClusters);
  cluster_params->set_max(kMaxNumberOfClusters);

  query->mutable_peek_text_params()->set_enabled(true);

  return pivot_request.SerializeAsString();
}

ServerExperimentInfos GetServerExperimentInfosFromResponse(
    const GetPivotsResponse& response_proto) {
  ServerExperimentInfos field_trials;
  for (auto experiment_info : response_proto.pivots().experiment_info()) {
    std::string trial_name = experiment_info.experiment_group_name();
    std::string group_name = experiment_info.experiment_arm_name();
    if (!trial_name.empty() && !group_name.empty())
      field_trials.emplace_back(std::move(trial_name), std::move(group_name));
  }

  return field_trials;
}

ContextualSuggestionsResult ResultFromResponse(
    const GetPivotsResponse& response_proto) {
  return ContextualSuggestionsResult(
      GetPeekTextFromResponse(response_proto),
      GetClustersFromResponse(response_proto),
      GetPeekConditionsFromResponse(response_proto),
      GetServerExperimentInfosFromResponse(response_proto));
}

}  // namespace

ContextualSuggestionsFetch::ContextualSuggestionsFetch(
    const GURL& url,
    const std::string& bcp_language_code,
    bool include_cookies)
    : url_(url),
      bcp_language_code_(bcp_language_code),
      include_cookies_(include_cookies) {}

ContextualSuggestionsFetch::~ContextualSuggestionsFetch() = default;

// static
const std::string ContextualSuggestionsFetch::GetFetchEndpoint() {
  std::string fetch_endpoint;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kFetchEndpointUrlKey)) {
    fetch_endpoint =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kFetchEndpointUrlKey);
  } else {
    fetch_endpoint = base::GetFieldTrialParamValueByFeature(
        kContextualSuggestionsButton, kFetchEndpointUrlKey);
  }

  if (!base::StartsWith(fetch_endpoint, "https://",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    fetch_endpoint = kDefaultFetchEndpointUrl;
  }

  fetch_endpoint.append(kFetchEndpointServicePath);

  return fetch_endpoint;
}

void ContextualSuggestionsFetch::Start(
    FetchClustersCallback callback,
    ReportFetchMetricsCallback metrics_callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& loader_factory) {
  request_completed_callback_ = std::move(callback);
  url_loader_ = MakeURLLoader();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory.get(),
      base::BindOnce(&ContextualSuggestionsFetch::OnURLLoaderComplete,
                     base::Unretained(this), std::move(metrics_callback)));
}

std::unique_ptr<network::SimpleURLLoader>
ContextualSuggestionsFetch::MakeURLLoader() const {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_contextual_suggestions_fetch",
                                          R"(
        semantics {
          sender: "Contextual Suggestions Fetch"
          description:
            "Chromium can show contextual suggestions that are related to the "
            "currently visited page."
          trigger:
            "Triggered when a page is visited and stayed on for more than a "
            "few seconds."
          data:
            "The URL of the current tab and the ui language."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled by turning off the "
            "'Suggest related pages' in Chrome for Android settings"
          policy_exception_justification: "Not implemented. The feature is "
          "currently Android-only and disabled for all enterprise users. "
          "A policy will be added before enabling for enterprise users."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = GURL(GetFetchEndpoint());

  int cookie_flag = include_cookies_ ? 0 : net::LOAD_DO_NOT_SEND_COOKIES;
  resource_request->load_flags = net::LOAD_BYPASS_CACHE |
                                 net::LOAD_DO_NOT_SAVE_COOKIES | cookie_flag |
                                 net::LOAD_DO_NOT_SEND_AUTH_DATA;
  resource_request->headers = MakeHeaders();
  resource_request->method = "GET";

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_loader->SetAllowHttpErrorResults(true);
  return simple_loader;
}

net::HttpRequestHeaders ContextualSuggestionsFetch::MakeHeaders() const {
  net::HttpRequestHeaders headers;
  std::string serialized_request_body =
      SerializedPivotsRequest(url_.spec(), bcp_language_code_);
  std::string base64_encoded_body;
  base::Base64UrlEncode(serialized_request_body,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &base64_encoded_body);
  headers.SetHeader("X-Protobuffer-Request-Payload", base64_encoded_body);
  variations::AppendVariationHeaders(url_, variations::InIncognito::kNo,
                                     variations::SignedIn::kNo, &headers);

  UMA_HISTOGRAM_COUNTS_1M(
      "ContextualSuggestions.FetchRequestProtoSizeKB",
      static_cast<int>(base64_encoded_body.length() / 1024));

  return headers;
}

void ContextualSuggestionsFetch::OnURLLoaderComplete(
    ReportFetchMetricsCallback metrics_callback,
    std::unique_ptr<std::string> result) {
  ContextualSuggestionsResult suggestions_result;

  if (result) {
    int32_t response_code =
        url_loader_->ResponseInfo()->headers->response_code();
    if (response_code == net::HTTP_OK) {
      // The response comes in the format (length, bytes) where length is a
      // varint32 encoded int. Rather than hand-rolling logic to skip the
      // length(which we don't actually need), we use EncodedInputStream to
      // skip past it, then parse the remainder of the stream.
      google::protobuf::io::CodedInputStream coded_stream(
          reinterpret_cast<const uint8_t*>(result->data()), result->length());
      uint32_t response_size;
      if (coded_stream.ReadVarint32(&response_size) && response_size != 0) {
        GetPivotsResponse response_proto;
        if (response_proto.MergePartialFromCodedStream(&coded_stream)) {
          suggestions_result = ResultFromResponse(response_proto);
        }
      }
    }
  }

  ReportFetchMetrics(suggestions_result.clusters.size(),
                     std::move(metrics_callback));
  std::move(request_completed_callback_).Run(std::move(suggestions_result));
}

void ContextualSuggestionsFetch::ReportFetchMetrics(
    size_t clusters_size,
    ReportFetchMetricsCallback metrics_callback) {
  int32_t error_code = url_loader_->NetError();
  int32_t response_code = 0;

  base::UmaHistogramSparse("ContextualSuggestions.FetchErrorCode", error_code);
  if (error_code == net::OK) {
    const network::ResourceResponseHead* response_info =
        url_loader_->ResponseInfo();
    response_code = response_info->headers->response_code();
    if (response_code > 0) {
      base::UmaHistogramSparse("ContextualSuggestions.FetchResponseCode",
                               response_code);

      UMA_HISTOGRAM_COUNTS_1M("ContextualSuggestions.FetchResponseNetworkBytes",
                              response_info->encoded_data_length);
    }

    base::TimeDelta latency_delta =
        response_info->response_time - response_info->request_time;
    UMA_HISTOGRAM_COUNTS_1M("ContextualSuggestions.FetchLatencyMilliseconds",
                            latency_delta.InMilliseconds());
  }

  ContextualSuggestionsEvent event;
  if (error_code != net::OK) {
    event = FETCH_ERROR;
  } else if (response_code == net::HTTP_SERVICE_UNAVAILABLE) {
    event = FETCH_SERVER_BUSY;
  } else if (response_code != net::HTTP_OK) {
    event = FETCH_ERROR;
  } else if (clusters_size == 0) {
    event = FETCH_EMPTY;
  } else {
    event = FETCH_COMPLETED;
  }

  std::move(metrics_callback).Run(event);
}

}  // namespace contextual_suggestions
