// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/cup_factory.h"
#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace autofill_assistant {

// static
std::unique_ptr<ServiceImpl> ServiceImpl::Create(
    content::BrowserContext* context,
    Client* client) {
  return ServiceImpl::Create(
      context, client,
      ServerUrlFetcher(ServerUrlFetcher::GetDefaultServerUrl()));
}

// static
std::unique_ptr<ServiceImpl> ServiceImpl::Create(
    content::BrowserContext* context,
    Client* client,
    const ServerUrlFetcher& url_fetcher) {
  auto request_sender = std::make_unique<ServiceRequestSenderImpl>(
      context, client->GetAccessTokenFetcher(),
      std::make_unique<cup::CUPImplFactory>(),
      std::make_unique<NativeURLLoaderFactory>(),
      ApiKeyFetcher().GetAPIKey(client->GetChannel()),
      /* auth_enabled = */ "false" !=
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAutofillAssistantAuth),
      /* disable_auth_if_no_access_token = */ true);

  return std::make_unique<ServiceImpl>(
      client, std::move(request_sender),
      url_fetcher.GetSupportsScriptEndpoint(),
      url_fetcher.GetNextActionsEndpoint(),
      std::make_unique<ClientContextImpl>(client));
}

ServiceImpl::ServiceImpl(Client* client,
                         std::unique_ptr<ServiceRequestSender> request_sender,
                         const GURL& script_server_url,
                         const GURL& action_server_url,
                         std::unique_ptr<ClientContext> client_context)
    : client_(client),
      request_sender_(std::move(request_sender)),
      script_server_url_(script_server_url),
      script_action_server_url_(action_server_url),
      client_context_(std::move(client_context)) {
  DCHECK(script_server_url.is_valid());
  DCHECK(action_server_url.is_valid());
}

ServiceImpl::~ServiceImpl() {}

void ServiceImpl::SetScriptStoreConfig(
    const ScriptStoreConfig& script_store_config) {
  script_store_config_ = script_store_config;
}

void ServiceImpl::GetScriptsForUrl(const GURL& url,
                                   const TriggerContext& trigger_context,
                                   ResponseCallback callback) {
  DCHECK(url.is_valid());
  client_context_->Update(trigger_context);
  request_sender_->SendRequest(script_server_url_,
                               ProtocolUtils::CreateGetScriptsRequest(
                                   url, client_context_->AsProto(),
                                   trigger_context.GetScriptParameters()),
                               std::move(callback), RpcType::SUPPORTS_SCRIPT);
}

void ServiceImpl::GetActions(const std::string& script_path,
                             const GURL& url,
                             const TriggerContext& trigger_context,
                             const std::string& global_payload,
                             const std::string& script_payload,
                             ResponseCallback callback) {
  DCHECK(!script_path.empty());
  client_context_->Update(trigger_context);
  if (client_context_->AsProto().payments_client_token().empty() &&
      base::FeatureList::IsEnabled(
          features::kAutofillAssistantGetPaymentsClientToken)) {
    client_->FetchPaymentsClientToken(base::BindOnce(
        &ServiceImpl::OnFetchPaymentsClientToken,
        weak_ptr_factory_.GetWeakPtr(), script_path, url,
        std::make_unique<TriggerContext>(
            std::vector<const TriggerContext*>{&trigger_context}),
        global_payload, script_payload, std::move(callback)));
  } else {
    SendGetActions(script_path, url, trigger_context, global_payload,
                   script_payload, std::move(callback));
  }
}

void ServiceImpl::OnFetchPaymentsClientToken(
    const std::string& script_path,
    const GURL& url,
    std::unique_ptr<TriggerContext> trigger_context,
    const std::string& global_payload,
    const std::string& script_payload,
    ResponseCallback callback,
    const std::string& client_token) {
  client_context_->SetPaymentsClientToken(client_token);
  SendGetActions(script_path, url, *trigger_context, global_payload,
                 script_payload, std::move(callback));
}

void ServiceImpl::SendGetActions(const std::string& script_path,
                                 const GURL& url,
                                 const TriggerContext& trigger_context,
                                 const std::string& global_payload,
                                 const std::string& script_payload,
                                 ResponseCallback callback) {
  request_sender_->SendRequest(
      script_action_server_url_,
      ProtocolUtils::CreateInitialScriptActionsRequest(
          script_path, url, global_payload, script_payload,
          client_context_->AsProto(), trigger_context.GetScriptParameters(),
          script_store_config_),
      std::move(callback), RpcType::GET_ACTIONS);
}

void ServiceImpl::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    const RoundtripTimingStats& timing_stats,
    ResponseCallback callback) {
  client_context_->Update(trigger_context);
  request_sender_->SendRequest(
      script_action_server_url_,
      ProtocolUtils::CreateNextScriptActionsRequest(
          previous_global_payload, previous_script_payload, processed_actions,
          timing_stats, client_context_->AsProto()),
      std::move(callback), RpcType::GET_ACTIONS);
}

}  // namespace autofill_assistant
