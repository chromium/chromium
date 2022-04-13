// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_assistant_impl.h"

#include <vector>

#include "components/autofill_assistant/browser/desktop/starter_delegate_desktop.h"
#include "components/autofill_assistant/browser/headless/external_script_controller_impl.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/cup_impl.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"
#include "components/autofill_assistant/browser/service/simple_url_loader_factory.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "net/http/http_status_code.h"

namespace autofill_assistant {

namespace {

const char kIntentScriptParameterKey[] = "INTENT";

void OnCapabilitiesResponse(
    AutofillAssistant::GetCapabilitiesResponseCallback callback,
    int http_status,
    const std::string& response_str,
    const ServiceRequestSender::ResponseInfo& response_info) {
  std::vector<AutofillAssistant::CapabilitiesInfo> infos;
  GetCapabilitiesByHashPrefixResponseProto resp;

  if (http_status != net::HTTP_OK) {
    VLOG(1) << "Failed to get script capabilities."
            << ", http-status=" << http_status;
    // TODO(b/209429727) Record network failure metrics.
    std::move(callback).Run(http_status, infos);
    return;
  }

  if (!resp.ParseFromString(response_str)) {
    LOG(ERROR) << __func__ << " returned unparsable response";
    // TODO(b/209429727) Record parsing failure metrics.
    std::move(callback).Run(http_status, infos);
    return;
  }

  for (const auto& match : resp.match_info()) {
    AutofillAssistant::CapabilitiesInfo info;
    info.url = match.url_match();

    for (const auto& param : match.script_parameters_override()) {
      info.script_parameters[param.name()] = param.value();
    }

    infos.push_back(info);
  }
  std::move(callback).Run(http_status, infos);
}

}  // namespace

// static
std::unique_ptr<AutofillAssistantImpl> AutofillAssistantImpl::Create(
    content::BrowserContext* browser_context,
    version_info::Channel channel,
    const std::string& country_code,
    const std::string& locale) {
  auto request_sender = std::make_unique<ServiceRequestSenderImpl>(
      browser_context,
      /* access_token_fetcher = */ nullptr,
      std::make_unique<cup::CUPImplFactory>(),
      std::make_unique<NativeURLLoaderFactory>(),
      ApiKeyFetcher().GetAPIKey(channel));
  const ServerUrlFetcher& url_fetcher =
      ServerUrlFetcher(ServerUrlFetcher::GetDefaultServerUrl());
  return std::make_unique<AutofillAssistantImpl>(
      std::move(request_sender), url_fetcher.GetCapabilitiesByHashEndpoint(),
      country_code, locale);
}

AutofillAssistantImpl::AutofillAssistantImpl(
    std::unique_ptr<ServiceRequestSender> request_sender,
    const GURL& script_server_url,
    const std::string& country_code,
    const std::string& locale)
    : request_sender_(std::move(request_sender)),
      script_server_url_(script_server_url),
      country_code_(country_code),
      locale_(locale) {}

AutofillAssistantImpl::~AutofillAssistantImpl() = default;

void AutofillAssistantImpl::GetCapabilitiesByHashPrefix(
    uint32_t hash_prefix_length,
    const std::vector<uint64_t>& hash_prefixes,
    const std::string& intent,
    GetCapabilitiesResponseCallback callback) {
  const ScriptParameters& parameters = {
      base::flat_map<std::string, std::string>{
          {kIntentScriptParameterKey, intent}}};

  ClientContextProto client_context;
  client_context.set_country(country_code_);
  client_context.set_locale(locale_);
  client_context.mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());

  request_sender_->SendRequest(
      script_server_url_,
      ProtocolUtils::CreateCapabilitiesByHashRequest(
          hash_prefix_length, hash_prefixes, client_context, parameters),
      ServiceRequestSender::AuthMode::API_KEY,
      base::BindOnce(&OnCapabilitiesResponse, std::move(callback)),
      RpcType::GET_CAPABILITIES_BY_HASH_PREFIX);
  return;
}

std::unique_ptr<ExternalScriptController>
AutofillAssistantImpl::CreateExternalScriptController(
    content::WebContents* web_contents) {
  return std::make_unique<ExternalScriptControllerImpl>(web_contents);
}

}  // namespace autofill_assistant
