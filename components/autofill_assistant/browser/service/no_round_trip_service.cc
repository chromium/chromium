// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/no_round_trip_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/local_script_store.h"
#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "url/origin.h"

namespace autofill_assistant {
using RoutineScript =
    GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript;

namespace {
constexpr uint32_t kHashPrefixLength = 15;

ClientContextProto GetClientContext() {
  // Should the ClientContext come from somewhere?
  ClientContextProto client_context;
  client_context.mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());
  return client_context;
}

bool AuthEnabled() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kAutofillAssistantAuth) == "true";
}

ServiceRequestSender::AuthMode GetDefaultAuthMode() {
  return AuthEnabled()
             ? ServiceRequestSender::AuthMode::OAUTH_WITH_API_KEY_FALLBACK
             : ServiceRequestSender::AuthMode::API_KEY;
}

std::vector<RoutineScript> CreateRoutines(
    const autofill_assistant::
        GetNoRoundTripScriptsByHashPrefixResponseProto_MatchInfo& match) {
  std::vector<RoutineScript> routines;
  routines.reserve(match.routine_scripts_size());
  for (const auto& routine_script : match.routine_scripts()) {
    routines.push_back(routine_script);
  }
  return routines;
}

std::unique_ptr<LocalScriptStore> CreateStoreFromMatch(
    const GetNoRoundTripScriptsByHashPrefixResponseProto_MatchInfo& match) {
  const std::string domain = match.domain();
  const std::vector<RoutineScript> routines = CreateRoutines(match);
  const SupportsScriptResponseProto supports_site_response =
      match.supports_site_response();
  return std::make_unique<LocalScriptStore>(routines, domain,
                                            supports_site_response);
}

}  // namespace

NoRoundTripService::NoRoundTripService(
    std::unique_ptr<ServiceRequestSender> request_sender,
    const GURL& get_scripts_endpoint,
    const GURL& progress_endpoint,
    Client* client)
    : get_scripts_endpoint_(get_scripts_endpoint),
      progress_endpoint_(progress_endpoint),
      client_(client),
      request_sender_(std::move(request_sender)) {}

NoRoundTripService::~NoRoundTripService() = default;

// static
std::string NoRoundTripService::CreateGetNoRoundtripRequest(
    const GURL& url,
    const ScriptParameters& script_parameters) {
  DCHECK(!url.is_empty());

  const auto hash_prefix = AutofillAssistant::GetHashPrefix(
      kHashPrefixLength, url::Origin::Create(url));
  const std::string request =
      ProtocolUtils::CreateGetNoRoundTripScriptsByHashRequest(
          kHashPrefixLength, hash_prefix, GetClientContext(),
          script_parameters);

  return request;
}

// static
std::unique_ptr<NoRoundTripService> NoRoundTripService::Create(
    content::BrowserContext* browser_context,
    Client* client) {
  return NoRoundTripService::Create(
      browser_context, client,
      ServerUrlFetcher(ServerUrlFetcher::GetDefaultServerUrl()));
}

// static
std::unique_ptr<NoRoundTripService> NoRoundTripService::Create(
    content::BrowserContext* browser_context,
    Client* client,
    const ServerUrlFetcher& url_fetcher) {
  auto request_sender = std::make_unique<ServiceRequestSenderImpl>(
      browser_context, client->GetAccessTokenFetcher(),
      std::make_unique<cup::CUPImplFactory>(),
      std::make_unique<NativeURLLoaderFactory>(),
      ApiKeyFetcher().GetAPIKey(client->GetChannel()));

  return std::make_unique<NoRoundTripService>(
      std::move(request_sender),
      url_fetcher.GetNoRoundTripScriptsByHashEndpoint(),
      url_fetcher.GetReportProgressEndpoint(), client);
}

void NoRoundTripService::SetScriptStoreConfig(
    const ScriptStoreConfig& script_store_config) {
  script_store_config_ = script_store_config;
}

void NoRoundTripService::GetScriptsForUrl(
    const GURL& url,
    const TriggerContext& trigger_context,
    ServiceRequestSender::ResponseCallback callback) {
  const std::string request =
      CreateGetNoRoundtripRequest(url, trigger_context.GetScriptParameters());

  request_sender_->SendRequest(
      get_scripts_endpoint_, request, GetDefaultAuthMode(),
      base::BindOnce(&NoRoundTripService::OnNoRountripByHashPrefixResponse,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)),
      RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX);
}

void NoRoundTripService::GetActions(
    const std::string& script_path,
    const GURL& url,
    const TriggerContext& trigger_context,
    const std::string& global_payload,
    const std::string& script_payload,
    ServiceRequestSender::ResponseCallback callback) {
  DCHECK(!script_path.empty());
  if (!script_store_) {
    LOG(ERROR) << __func__ << " called on an empty script store.";
    std::move(callback).Run(net::HTTP_BAD_REQUEST, "", {});
    return;
  }

  for (const auto& routine : script_store_->GetRoutines()) {
    if (!routine.has_script_path() || routine.script_path() != script_path) {
      continue;
    }

    const std::string get_actions_response =
        routine.action_response().SerializeAsString();
    const ServiceRequestSender::ResponseInfo response_info{
        .encoded_body_length =
            static_cast<int64_t>(routine.action_response().ByteSizeLong())};

    std::move(callback).Run(net::HTTP_OK, get_actions_response, response_info);
    return;
  }

  std::move(callback).Run(net::HTTP_BAD_REQUEST, "", {});
}

void NoRoundTripService::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    const RoundtripTimingStats& timing_stats,
    const RoundtripNetworkStats& network_stats,
    ServiceRequestSender::ResponseCallback callback) {
  if (!script_store_) {
    LOG(ERROR) << __func__ << " called on an empty script store.";
    std::move(callback).Run(net::HTTP_BAD_REQUEST, "", {});
    return;
  }

  LOG(WARNING) << __func__
               << "called in NoRoundTripService, returning empty list";
  std::move(callback).Run(net::HTTP_OK, "",
                          ServiceRequestSender::ResponseInfo());
}

void NoRoundTripService::GetUserData(
    const CollectUserDataOptions& options,
    uint64_t run_id,
    const UserData* user_data,
    ServiceRequestSender::ResponseCallback callback) {
  LOG(ERROR) << __func__ << "not available in NoRoundTripService";
  std::move(callback).Run(net::HTTP_METHOD_NOT_ALLOWED, "",
                          ServiceRequestSender::ResponseInfo());
}

void NoRoundTripService::SetDisableRpcSigning(bool disable_rpc_signing) {
  LOG(WARNING) << __func__ << "not available in NoRoundTripService";
}

void NoRoundTripService::UpdateAnnotateDomModelContext(int64_t model_version) {
  LOG(WARNING) << __func__ << "not available in NoRoundTripService";
}

void NoRoundTripService::UpdateJsFlowLibraryLoaded(
    const bool js_flow_library_loaded) {
  LOG(WARNING) << __func__ << "not available in NoRoundTripService";
}

void NoRoundTripService::ReportProgress(
    const std::string& token,
    const std::string& payload,
    ServiceRequestSender::ResponseCallback callback) {
  if (!client_->GetMakeSearchesAndBrowsingBetterEnabled() ||
      !client_->GetMetricsReportingEnabled()) {
    return;
  }
  request_sender_->SendRequest(
      progress_endpoint_,
      ProtocolUtils::CreateReportProgressRequest(token, payload),
      GetDefaultAuthMode(), std::move(callback), RpcType::REPORT_PROGRESS);
}

void NoRoundTripService::OnNoRountripByHashPrefixResponse(
    const GURL& url,
    ServiceRequestSender::ResponseCallback callback,
    int http_status,
    const std::string& response,
    const ServiceRequestSender::ResponseInfo& response_info) {
  GetNoRoundTripScriptsByHashPrefixResponseProto resp;
  if (http_status != net::HTTP_OK) {
    LOG(ERROR) << " Failed to get scripts."
               << ", http-status=" << http_status;
    std::move(callback).Run(http_status, "", {});
    return;
  }

  if (!resp.ParseFromString(response)) {
    LOG(ERROR) << __func__ << " returned unparsable response";
    std::move(callback).Run(net::HTTP_INTERNAL_SERVER_ERROR, "", {});
    return;
  }

  std::vector<std::string> extra_urls;
  for (const GetNoRoundTripScriptsByHashPrefixResponseProto_MatchInfo& match :
       resp.match_infos()) {
    VLOG(3) << "Parsing " << match.domain();
    if (url.host() != GURL(match.domain()).host()) {
      continue;
    }
    script_store_ = CreateStoreFromMatch(match);
    std::move(callback).Run(
        net::HTTP_OK,
        script_store_->GetSupportsSiteResponse().SerializeAsString(),
        {.encoded_body_length = static_cast<int64_t>(
             script_store_->GetSupportsSiteResponse().ByteSizeLong())});
    return;
  }

  LOG(ERROR) << __func__ << " could not find a matching url.";
#if DEBUG
  for (const auto& match : resp.match_infos()) {
    extra_urls.push_back(match.domain());
  }
  LOG(ERROR) << __func__ << "Looking for " << url.HostNoBrackets()
             << ", found: " << base::JoinString(extra_urls, " ");
#endif
  std::move(callback).Run(net::HTTP_BAD_REQUEST, "", {});
}

NoRoundTripService::NoRoundTripService(
    std::unique_ptr<LocalScriptStore> script_store)
    : script_store_(std::move(script_store)) {}

const LocalScriptStore* NoRoundTripService::GetStore() const {
  return script_store_.get();
}

}  // namespace autofill_assistant
