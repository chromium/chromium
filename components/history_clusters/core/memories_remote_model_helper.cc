// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_remote_model_helper.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/proto/clusters.pb.h"

namespace history_clusters {

namespace {

const size_t kMaxExpectedResponseSize = 1024 * 1024;

// Also writes one line of debug information per visit to `debug_string`, if
// the parameter is non-nullptr.
proto::GetClustersRequest CreateRequestProto(
    const std::vector<history::AnnotatedVisit>& visits,
    absl::optional<DebugLoggerCallback> debug_logger) {
  proto::GetClustersRequest request;
  request.set_experiment_name(kRemoteModelEndpointExperimentName.Get());

  base::ListValue debug_visits_list;
  for (auto& visit : visits) {
    proto::Visit* request_visit = request.add_visits();
    request_visit->set_visit_id(visit.visit_row.visit_id);
    request_visit->set_url(visit.url_row.url().spec());
    request_visit->set_origin(visit.url_row.url().GetOrigin().spec());
    request_visit->set_navigation_time_ms(
        visit.visit_row.visit_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
    request_visit->set_page_end_reason(
        visit.context_annotations.page_end_reason);
    request_visit->set_page_transition(
        static_cast<int>(visit.visit_row.transition));

    if (debug_logger) {
      base::DictionaryValue debug_visit;
      debug_visit.SetStringKey("visitId",
                               base::NumberToString(request_visit->visit_id()));
      debug_visit.SetStringKey("url", request_visit->url());
      debug_visit.SetStringKey(
          "navigationTimeMs",
          base::NumberToString(request_visit->navigation_time_ms()));
      debug_visit.SetStringKey(
          "pageEndReason",
          base::NumberToString(request_visit->page_end_reason()));
      debug_visit.SetStringKey(
          "pageTransition",
          base::NumberToString(request_visit->page_transition()));
      debug_visits_list.Append(std::move(debug_visit));
    }
  }

  if (debug_logger) {
    debug_logger->Run("MemoriesRemoteModelHelper CreateRequestProto:");

    base::DictionaryValue debug_value;
    debug_value.SetStringKey("experiment_name", request.experiment_name());
    debug_value.SetKey("visits", std::move(debug_visits_list));

    std::string debug_string;
    if (base::JSONWriter::WriteWithOptions(
            debug_value, base::JSONWriter::OPTIONS_PRETTY_PRINT,
            &debug_string)) {
      debug_logger->Run(debug_string);
    }
  }
  return request;
}

std::vector<history::Cluster> ParseResponseProto(
    const std::vector<history::AnnotatedVisit>& visits,
    const proto::GetClustersResponse& response_proto,
    absl::optional<DebugLoggerCallback> debug_logger) {
  std::vector<history::Cluster> clusters;
  for (const proto::Cluster& cluster_proto : response_proto.clusters()) {
    history::Cluster cluster;
    for (const std::string& keyword : cluster_proto.keywords())
      cluster.keywords.push_back(base::UTF8ToUTF16(keyword));
    for (int64_t visit_id : cluster_proto.visit_ids()) {
      const auto visits_it = base::ranges::find(
          visits, visit_id,
          [](const auto& visit) { return visit.visit_row.visit_id; });
      if (visits_it != visits.end())
        cluster.annotated_visits.push_back(*visits_it);
    }
    clusters.push_back(cluster);
  }

  if (debug_logger) {
    // TODO(manukh): `ListValue` is deprecated; replace with `std::vector`.
    base::ListValue debug_clusters_list;
    for (const proto::Cluster& cluster : response_proto.clusters()) {
      base::DictionaryValue debug_cluster;

      base::ListValue debug_keywords;
      for (const std::string& keyword : cluster.keywords()) {
        debug_keywords.Append(keyword);
      }
      debug_cluster.SetKey("keywords", std::move(debug_keywords));

      base::ListValue debug_visit_ids;
      for (int64_t visit_id : cluster.visit_ids()) {
        debug_visit_ids.Append(base::NumberToString(visit_id));
      }
      debug_cluster.SetKey("visit_ids", std::move(debug_visit_ids));

      debug_clusters_list.Append(std::move(debug_cluster));
    }

    debug_logger->Run("MemoriesRemoteModelHelper ParseResponseProto Clusters:");

    std::string debug_string;
    if (base::JSONWriter::WriteWithOptions(
            debug_clusters_list, base::JSONWriter::OPTIONS_PRETTY_PRINT,
            &debug_string)) {
      debug_logger->Run(debug_string);
    }
  }

  return clusters;
}

}  // namespace

MemoriesRemoteModelHelper::MemoriesRemoteModelHelper(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    absl::optional<DebugLoggerCallback> debug_logger)
    : url_loader_factory_(url_loader_factory), debug_logger_(debug_logger) {}

MemoriesRemoteModelHelper::~MemoriesRemoteModelHelper() = default;

void MemoriesRemoteModelHelper::GetMemories(
    MemoriesCallback callback,
    const std::vector<history::AnnotatedVisit>& visits) {
  const GURL endpoint(RemoteModelEndpoint());
  if (!endpoint.is_valid() || visits.empty()) {
    std::move(callback).Run({});
    return;
  }

  // It's weird but the endpoint only accepts JSON, so wrap our serialized proto
  // like this: {"data":"<base64-encoded-proto-serialization>"}
  proto::GetClustersRequest request_proto =
      CreateRequestProto(visits, debug_logger_);
  const std::string serialized_request_proto =
      request_proto.SerializeAsString();
  std::string request_proto_base64;
  base::Base64Encode(serialized_request_proto, &request_proto_base64);

  base::DictionaryValue container_value;
  container_value.SetStringPath("data", request_proto_base64);

  std::string request_body;
  base::JSONWriter::Write(container_value, &request_body);

  auto url_loader = CreateLoader(CreateRequest(endpoint), request_body);
  network::SimpleURLLoader* unowned_url_loader = url_loader.get();
  unowned_url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](std::unique_ptr<network::SimpleURLLoader> url_loader,
             absl::optional<DebugLoggerCallback> debug_logger,
             const std::vector<history::AnnotatedVisit>& visits,
             std::unique_ptr<std::string> response) {
            if (!response) {
              if (debug_logger) {
                debug_logger->Run("MemoriesRemoteModelHelper response nullptr");
              }
              return std::vector<history::Cluster>{};
            }
            proto::GetClustersResponse response_proto;
            response_proto.ParseFromString(*response);
            return ParseResponseProto(visits, response_proto, debug_logger);
          },
          std::move(url_loader), debug_logger_, visits)
          .Then(std::move(callback)),
      kMaxExpectedResponseSize);
}

// static
std::unique_ptr<network::ResourceRequest>
MemoriesRemoteModelHelper::CreateRequest(const GURL& endpoint) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(endpoint);
  request->method = "POST";
  return request;
}

// static
std::unique_ptr<network::SimpleURLLoader>
MemoriesRemoteModelHelper::CreateLoader(
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
