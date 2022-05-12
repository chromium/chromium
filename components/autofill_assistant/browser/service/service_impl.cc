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
namespace {

bool AuthEnabled() {
  return "false" != base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                        switches::kAutofillAssistantAuth);
}

ServiceRequestSender::AuthMode GetDefaultAuthMode() {
  return AuthEnabled()
             ? ServiceRequestSender::AuthMode::OAUTH_WITH_API_KEY_FALLBACK
             : ServiceRequestSender::AuthMode::API_KEY;
}

}  // namespace

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
      ApiKeyFetcher().GetAPIKey(client->GetChannel()));

  return std::make_unique<ServiceImpl>(
      client, std::move(request_sender),
      url_fetcher.GetSupportsScriptEndpoint(),
      url_fetcher.GetNextActionsEndpoint(), url_fetcher.GetUserDataEndpoint(),
      std::make_unique<ClientContextImpl>(client));
}

ServiceImpl::ServiceImpl(Client* client,
                         std::unique_ptr<ServiceRequestSender> request_sender,
                         const GURL& script_server_url,
                         const GURL& action_server_url,
                         const GURL& user_data_url,
                         std::unique_ptr<ClientContext> client_context)
    : client_(client),
      request_sender_(std::move(request_sender)),
      script_server_url_(script_server_url),
      script_action_server_url_(action_server_url),
      user_data_url_(user_data_url),
      client_context_(std::move(client_context)) {
  DCHECK(script_server_url.is_valid());
  DCHECK(action_server_url.is_valid());
  DCHECK(user_data_url_.is_valid());
}

ServiceImpl::~ServiceImpl() {}

void ServiceImpl::SetScriptStoreConfig(
    const ScriptStoreConfig& script_store_config) {
  script_store_config_ = script_store_config;
}

void ServiceImpl::GetScriptsForUrl(
    const GURL& url,
    const TriggerContext& trigger_context,
    ServiceRequestSender::ResponseCallback callback) {
  DCHECK(url.is_valid());
  client_context_->Update(trigger_context);
  request_sender_->SendRequest(script_server_url_,
                               ProtocolUtils::CreateGetScriptsRequest(
                                   url, client_context_->AsProto(),
                                   trigger_context.GetScriptParameters()),
                               GetDefaultAuthMode(), std::move(callback),
                               RpcType::SUPPORTS_SCRIPT);
}

void ServiceImpl::GetActions(const std::string& script_path,
                             const GURL& url,
                             const TriggerContext& trigger_context,
                             const std::string& global_payload,
                             const std::string& script_payload,
                             ServiceRequestSender::ResponseCallback callback) {
  DCHECK(!script_path.empty());
  client_context_->Update(trigger_context);
  request_sender_->SendRequest(
      script_action_server_url_,
      ProtocolUtils::CreateInitialScriptActionsRequest(
          script_path, url, global_payload, script_payload,
          client_context_->AsProto(), trigger_context.GetScriptParameters(),
          script_store_config_),
      GetDefaultAuthMode(), std::move(callback), RpcType::GET_ACTIONS);
}

void ServiceImpl::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    const RoundtripTimingStats& timing_stats,
    const RoundtripNetworkStats& network_stats,
    ServiceRequestSender::ResponseCallback callback) {
  client_context_->Update(trigger_context);
  request_sender_->SendRequest(
      script_action_server_url_,
      ProtocolUtils::CreateNextScriptActionsRequest(
          previous_global_payload, previous_script_payload, processed_actions,
          timing_stats, network_stats, client_context_->AsProto()),
      GetDefaultAuthMode(), std::move(callback), RpcType::GET_ACTIONS);
}

void ServiceImpl::GetUserData(const CollectUserDataOptions& options,
                              uint64_t run_id,
                              const UserData* user_data,
                              ServiceRequestSender::ResponseCallback callback) {
  std::vector<std::string> preexisting_address_ids;
  std::vector<std::string> preexisting_payment_instrument_ids;
  if (user_data) {
    for (const auto& address : user_data->available_addresses_) {
      if (address->identifier.has_value()) {
        preexisting_address_ids.emplace_back(*address->identifier);
      }
    }
    for (const auto& instrument : user_data->available_payment_instruments_) {
      if (instrument->identifier.has_value()) {
        preexisting_payment_instrument_ids.emplace_back(
            *instrument->identifier);
      }
    }
  }

  if (options.request_payment_method) {
    // We do not cache the payments client token. It could go stale (in practice
    // it currently doesn't). Getting the token is little overhead.
    client_->FetchPaymentsClientToken(base::BindOnce(
        &ServiceImpl::SendUserDataRequest, weak_ptr_factory_.GetWeakPtr(),
        run_id, options.request_payer_name, options.request_payer_email,
        options.request_payer_phone || options.request_phone_number_separately,
        options.request_shipping, preexisting_address_ids,
        options.request_payment_method, options.supported_basic_card_networks,
        preexisting_payment_instrument_ids, std::move(callback)));
    return;
  }

  SendUserDataRequest(
      run_id, options.request_payer_name, options.request_payer_email,
      options.request_payer_phone || options.request_phone_number_separately,
      options.request_shipping, preexisting_address_ids,
      options.request_payment_method, options.supported_basic_card_networks,
      preexisting_payment_instrument_ids, std::move(callback), std::string());
}

void ServiceImpl::SetDisableRpcSigning(bool disable_rpc_signing) {
  request_sender_->SetDisableRpcSigning(disable_rpc_signing);
}

void ServiceImpl::SendUserDataRequest(
    uint64_t run_id,
    bool request_name,
    bool request_email,
    bool request_phone,
    bool request_shipping,
    const std::vector<std::string>& preexisting_address_ids,
    bool request_payment_methods,
    const std::vector<std::string>& supported_card_networks,
    const std::vector<std::string>& preexisting_payment_instrument_ids,
    ServiceRequestSender::ResponseCallback callback,
    const std::string& client_token) {
  request_sender_->SendRequest(
      user_data_url_,
      ProtocolUtils::CreateGetUserDataRequest(
          run_id, request_name, request_email, request_phone, request_shipping,
          preexisting_address_ids, request_payment_methods,
          supported_card_networks, preexisting_payment_instrument_ids,
          client_token),
      ServiceRequestSender::AuthMode::OAUTH_STRICT, std::move(callback),
      RpcType::GET_USER_DATA);
}

}  // namespace autofill_assistant
