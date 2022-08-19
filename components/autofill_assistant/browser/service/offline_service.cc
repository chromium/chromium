// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/offline_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "net/http/http_status_code.h"

namespace autofill_assistant {

LocalScriptStore::LocalScriptStore(
    std::vector<GetNoRoundtripScriptsByHashPrefixResponseProto::MatchInfo::
                    RoutineScript> routines,
    std::string domain,
    SupportsScriptResponseProto supports_site_response)
    : routines(routines),
      domain(domain),
      supports_site_response(supports_site_response) {}
LocalScriptStore::LocalScriptStore(const LocalScriptStore&) = default;
LocalScriptStore::LocalScriptStore(LocalScriptStore&&) = default;
LocalScriptStore::~LocalScriptStore() = default;

// static
std::unique_ptr<OfflineService> OfflineService::Create(
    const LocalScriptStore& script_store) {
  return std::make_unique<OfflineService>(script_store);
}

OfflineService::OfflineService(const LocalScriptStore& script_store)
    : script_store_(script_store) {}

OfflineService::~OfflineService() = default;

void OfflineService::SetScriptStoreConfig(
    const ScriptStoreConfig& script_store_config) {
  script_store_config_ = script_store_config;
}

void OfflineService::GetScriptsForUrl(
    const GURL& url,
    const TriggerContext& trigger_context,
    ServiceRequestSender::ResponseCallback callback) {
  const std::string supports_site_response_str =
      script_store_.supports_site_response.SerializeAsString();

  const auto response_info = ServiceRequestSender::ResponseInfo(
      {.encoded_body_length = static_cast<int64_t>(
           script_store_.supports_site_response.ByteSizeLong())});

  const int status =
      url.DomainIs(script_store_.domain) ? net::HTTP_OK : net::HTTP_BAD_REQUEST;

  std::move(callback).Run(status, supports_site_response_str, response_info);
}

void OfflineService::GetActions(
    const std::string& script_path,
    const GURL& url,
    const TriggerContext& trigger_context,
    const std::string& global_payload,
    const std::string& script_payload,
    ServiceRequestSender::ResponseCallback callback) {
  DCHECK(!script_path.empty());

  ServiceRequestSender::ResponseInfo response_info;
  response_info.encoded_body_length = 0;

  for (const auto& routine : script_store_.routines) {
    if (routine.routine().path() != script_path) {
      continue;
    }

    std::string get_actions_response;
    routine.autobot_response().SerializeToString(&get_actions_response);
    response_info.encoded_body_length =
        static_cast<int64_t>(routine.autobot_response().ByteSizeLong());
    std::move(callback).Run(net::HTTP_OK, get_actions_response, response_info);
    return;
  }

  std::move(callback).Run(net::HTTP_BAD_REQUEST, "", response_info);
}

void OfflineService::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    const RoundtripTimingStats& timing_stats,
    const RoundtripNetworkStats& network_stats,
    ServiceRequestSender::ResponseCallback callback) {
  VLOG(1) << __func__ << "called in OfflineService, returning empty list";
  std::move(callback).Run(net::HTTP_OK, "",
                          ServiceRequestSender::ResponseInfo());
}

void OfflineService::GetUserData(
    const CollectUserDataOptions& options,
    uint64_t run_id,
    const UserData* user_data,
    ServiceRequestSender::ResponseCallback callback) {
  LOG(ERROR) << __func__ << "not available in OfflineService";
  std::move(callback).Run(net::HTTP_METHOD_NOT_ALLOWED, "",
                          ServiceRequestSender::ResponseInfo());
}

void OfflineService::SetDisableRpcSigning(bool disable_rpc_signing) {
  LOG(WARNING) << __func__ << "not available in OfflineService";
}

void OfflineService::UpdateAnnotateDomModelContext(int64_t model_version) {
  LOG(WARNING) << __func__ << "not available in OfflineService";
}

void OfflineService::UpdateJsFlowLibraryLoaded(
    const bool js_flow_library_loaded) {
  LOG(WARNING) << __func__ << "not available in OfflineService";
}

void OfflineService::ReportProgress(
    const std::string& token,
    const std::string& payload,
    ServiceRequestSender::ResponseCallback callback) {}

}  // namespace autofill_assistant
