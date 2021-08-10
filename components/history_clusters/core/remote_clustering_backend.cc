// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/remote_clustering_backend.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/proto/clusters.pb.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace history_clusters {

namespace {

// Also writes one line of debug information per visit to `debug_string`, if
// the parameter is non-nullptr.
proto::GetClustersRequest CreateRequestProto(
    const std::vector<history::AnnotatedVisit>& visits) {
  proto::GetClustersRequest request;
  request.set_experiment_name(kRemoteModelEndpointExperimentName.Get());

  for (auto& visit : visits) {
    // TODO(tommycli): Still need to set `site_engagement_score` and
    //  `is_from_google_search`
    proto::AnnotatedVisit* request_visit = request.add_visits();
    request_visit->set_visit_id(visit.visit_row.visit_id);
    request_visit->set_url(visit.url_row.url().spec());
    request_visit->set_origin(visit.url_row.url().GetOrigin().spec());
    request_visit->set_foreground_time_secs(
        visit.visit_row.visit_duration.InSeconds());
    request_visit->set_navigation_time_ms(
        visit.visit_row.visit_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
    request_visit->set_page_end_reason(
        visit.context_annotations.page_end_reason);
    request_visit->set_page_transition(
        static_cast<int>(visit.visit_row.transition));
    request_visit->set_referring_visit_id(
        visit.referring_visit_of_redirect_chain_start);
  }
  return request;
}

std::vector<history::Cluster> ParseResponseProto(
    const std::vector<history::AnnotatedVisit>& visits,
    const proto::GetClustersResponse& response_proto) {
  std::vector<history::Cluster> clusters;
  for (const proto::Cluster& cluster_proto : response_proto.clusters()) {
    history::Cluster cluster;
    for (const std::string& keyword : cluster_proto.keywords())
      cluster.keywords.push_back(base::UTF8ToUTF16(keyword));
    for (const proto::ClusterVisit& cluster_visit :
         cluster_proto.cluster_visits()) {
      const auto visits_it = base::ranges::find(
          visits, cluster_visit.visit_id(),
          [](const auto& visit) { return visit.visit_row.visit_id; });
      if (visits_it != visits.end()) {
        cluster.scored_annotated_visits.push_back(
            {*visits_it, cluster_visit.score()});
      }
    }
    clusters.push_back(cluster);
  }

  return clusters;
}

}  // namespace

RemoteClusteringBackend::RemoteClusteringBackend(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    absl::optional<DebugLoggerCallback> debug_logger)
    : url_loader_factory_(url_loader_factory), debug_logger_(debug_logger) {}

RemoteClusteringBackend::~RemoteClusteringBackend() = default;

void RemoteClusteringBackend::GetClusters(
    ClustersCallback callback,
    const std::vector<history::AnnotatedVisit>& visits) {
  const GURL endpoint(RemoteModelEndpoint());
  DCHECK(endpoint.is_valid());
  if (debug_logger_)
    debug_logger_->Run("RemoteClusteringBackend::GetClusters()");

  if (visits.empty()) {
    std::move(callback).Run({});
    return;
  }

  // It's weird but the endpoint only accepts JSON, so wrap our serialized proto
  // like this: {"data":"<base64-encoded-proto-serialization>"}
  proto::GetClustersRequest request_proto = CreateRequestProto(visits);
  const std::string serialized_request_proto =
      request_proto.SerializeAsString();
  std::string request_proto_base64;
  base::Base64Encode(serialized_request_proto, &request_proto_base64);

  base::DictionaryValue container_value;
  container_value.SetStringPath("data", request_proto_base64);

  std::string request_body;
  base::JSONWriter::Write(container_value, &request_body);

  // Also dump the encoded request, as it allows us to repro server-side errors.
  if (debug_logger_)
    debug_logger_->Run(request_body);

  auto url_loader = CreateLoader(CreateRequest(endpoint), request_body);
  network::SimpleURLLoader* unowned_url_loader = url_loader.get();

  // Retry 3 times (chosen arbitrarily) for transient-type network issues.
  int retry_mode =
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE |
      network::SimpleURLLoader::RetryMode::RETRY_ON_NAME_NOT_RESOLVED;
  unowned_url_loader->SetRetryOptions(3, retry_mode);

  unowned_url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](std::unique_ptr<network::SimpleURLLoader> url_loader,
             absl::optional<DebugLoggerCallback> debug_logger,
             const std::vector<history::AnnotatedVisit>& visits,
             std::unique_ptr<std::string> response) {
            if (!response) {
              if (debug_logger) {
                debug_logger->Run("RemoteClusteringBackend response nullptr");
                debug_logger->Run(base::StringPrintf("Net Error Code: %d",
                                                     url_loader->NetError()));
                debug_logger->Run("Net Error String: " +
                                  net::ErrorToString(url_loader->NetError()));
                if (url_loader->ResponseInfo() &&
                    url_loader->ResponseInfo()->headers) {
                  debug_logger->Run(base::StringPrintf(
                      "HTTP response code: %d",
                      url_loader->ResponseInfo()->headers->response_code()));
                }
              }
              return std::vector<history::Cluster>{};
            }
            proto::GetClustersResponse response_proto;
            response_proto.ParseFromString(*response);
            return ParseResponseProto(visits, response_proto);
          },
          std::move(url_loader), debug_logger_, visits)
          .Then(std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

// static
std::unique_ptr<network::ResourceRequest>
RemoteClusteringBackend::CreateRequest(const GURL& endpoint) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(endpoint);
  request->method = "POST";
  return request;
}

// static
std::unique_ptr<network::SimpleURLLoader> RemoteClusteringBackend::CreateLoader(
    std::unique_ptr<network::ResourceRequest> request,
    const std::string& request_body) {
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("memories_remote_model_request", R"(
        semantics {
          sender: "Memories"
          description:
            "chrome://memories clusters the user's history entries into "
            "'memories' for easier browsing and searching. This request is "
            "made only if the user explicitly sets the appropriate feature and "
            "param through the command line. The param is intentionally not in "
            "chrome://flags to avoid users accidentally setting it. The "
            "request will send the user's previous navigations and searches. "
          trigger: "User must set a command line param. The request is sent "
            "for both explicit user actions (e.g. navigating, visiting "
            "chrome://memories, or typing in the omnibox) and passively (e.g. "
            " on browser startup or periodically)."
          data: "The user's navigation and search history."
          destination: OTHER
          destination_other: "With the expected param configuration, the "
            "destination will be a GOOGLE_OWNED_SERVICE. As the endpoint is "
            "set through a command line param, a user could set it to any "
            "destination."
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Disabled unless the user explicitly enables the 'Memories' "
            "feature and sets the 'MemoriesRemoteModelEndpoint' param to a "
            "valid URL."
          policy_exception_justification:
            "The request is only made if the user explicitly configures it."
        })");
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  if (!request_body.empty())
    loader->AttachStringForUpload(request_body, "application/json");
  return loader;
}

}  // namespace history_clusters
