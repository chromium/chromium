// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_remote_model_helper.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/proto/clusters.pb.h"

namespace history_clusters {

namespace {

const size_t kMaxExpectedResponseSize = 1024 * 1024;

proto::GetClustersRequest CreateRequestProto(
    const std::vector<MemoriesVisit>& visits) {
  proto::GetClustersRequest request;
  for (auto& visit : visits) {
    proto::Visit* request_visit = request.add_visits();
    request_visit->set_visit_id(visit.visit_row.visit_id);
    request_visit->set_url(visit.url_row.url().spec());
    request_visit->set_origin(visit.url_row.url().GetOrigin().spec());
    request_visit->set_navigation_time_ms(
        visit.visit_row.visit_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
    request_visit->set_page_end_reason(visit.context_signals.page_end_reason);
    request_visit->set_page_transition(
        static_cast<int>(visit.visit_row.transition));

    // TODO(manukh) fill out:
    //  |foreground_time_secs|
    //  |site_engagement_score|
    //  |is_from_google_search|
  }
  return request;
}

mojom::VisitPtr CreateVisitMojom(const std::vector<MemoriesVisit>& visits,
                                 int64_t visit_id) {
  auto visit = mojom::Visit::New();
  visit->id = visit_id;
  const auto memory_visit_it = base::ranges::find(
      visits, visit->id,
      [](const auto& visit) { return visit.visit_row.visit_id; });
  if (memory_visit_it != visits.end()) {
    visit->url = memory_visit_it->url_row.url();
    visit->time = memory_visit_it->visit_row.visit_time;
    visit->page_title = base::UTF16ToUTF8(memory_visit_it->url_row.title());
  }

  // TODO(manukh) fill out:
  //  |thumbnail_url|
  //  |relative_date|
  //  |time_of_day|
  //  |num_duplicate_visits|
  //  |related_visits|
  //  |engagement_score|
  return visit;
}

Memories ParseResponseProto(const std::vector<MemoriesVisit>& visits,
                            const proto::GetClustersResponse& response_proto) {
  Memories result;
  for (const proto::Cluster& cluster : response_proto.clusters()) {
    auto memory = mojom::Memory::New();
    memory->id = base::UnguessableToken::Create();
    for (const std::string& keyword : cluster.keywords()) {
      memory->keywords.push_back(base::UTF8ToUTF16(keyword));
    }

    for (int64_t visit_id : cluster.visit_ids()) {
      memory->top_visits.push_back(CreateVisitMojom(visits, visit_id));
    }

    // TODO(manukh) fill out:
    //  |related_searches|
    //  |related_tab_groups|
    //  |bookmarks|
    //  |last_visit_time|
    //  |engagement_score|

    result.emplace_back(std::move(memory));
  }
  return result;
}

}  // namespace

MemoriesRemoteModelHelper::MemoriesRemoteModelHelper(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<void(const std::string&)> debug_logger)
    : url_loader_(nullptr),
      url_loader_factory_(url_loader_factory),
      debug_logger_(debug_logger) {}

MemoriesRemoteModelHelper::~MemoriesRemoteModelHelper() = default;

void MemoriesRemoteModelHelper::GetMemories(
    const std::vector<MemoriesVisit>& visits,
    MemoriesCallback callback) {
  const GURL endpoint(RemoteModelEndpointForDebugging());
  if (!endpoint.is_valid() || visits.empty()) {
    std::move(callback).Run({});
    return;
  }
  StopPendingRequests();

  // It's weird but the endpoint only accepts JSON, so wrap our serialized proto
  // like this: {"data":"<base64-encoded-proto-serialization>"}
  proto::GetClustersRequest request_proto = CreateRequestProto(visits);
  std::string request_proto_base64;
  base::Base64Encode(request_proto.SerializeAsString(), &request_proto_base64);

  base::DictionaryValue container_value;
  container_value.SetStringPath("data", request_proto_base64);

  std::string request_body;
  base::JSONWriter::Write(container_value, &request_body);

  auto request = CreateRequest(endpoint);
  debug_logger_.Run(
      base::StringPrintf("MemoriesRemoteModelHelper::GetMemories request = %s",
                         request_body.c_str()));
  url_loader_ = CreateLoader(CreateRequest(endpoint), request_body);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](base::RepeatingCallback<void(const std::string&)> debug_logger,
             const std::vector<MemoriesVisit>& visits,
             std::unique_ptr<std::string> response) {
            debug_logger.Run(base::StringPrintf(
                "MemoriesRemoteModelHelper::GetMemories response = %s",
                response ? response->c_str() : "nullptr"));
            if (!response) {
              return history_clusters::Memories();
            }
            proto::GetClustersResponse response_proto;
            response_proto.ParseFromString(*response);
            return ParseResponseProto(visits, response_proto);
          },
          debug_logger_, visits)
          .Then(std::move(callback)),
      kMaxExpectedResponseSize);
}

void MemoriesRemoteModelHelper::StopPendingRequests() {
  // TODO(manukh): Ensure the callback for the pending request is invoked.
  url_loader_.reset();
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
