// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
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

net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("autofill_service", R"(
        semantics {
          sender: "Autofill Assistant"
          description:
            "Chromium posts requests to autofill assistant server to get
            scripts for a URL."
          trigger:
            "Matching URL."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled in settings."
          policy_exception_justification: "Not implemented."
        })");

}  // namespace

// static
std::unique_ptr<ServiceImpl> ServiceImpl::Create(
    content::BrowserContext* context,
    Client* client) {
  ServerUrlFetcher url_fetcher{ServerUrlFetcher::GetDefaultServerUrl()};
  return std::make_unique<ServiceImpl>(
      ApiKeyFetcher().GetAPIKey(client->GetChannel()),
      url_fetcher.GetSupportsScriptEndpoint(),
      url_fetcher.GetNextActionsEndpoint(), context,
      std::make_unique<ClientContextImpl>(client),
      client->GetAccessTokenFetcher(),
      /* auth_enabled = */ "false" !=
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAutofillAssistantAuth));
}

ServiceImpl::ServiceImpl(const std::string& api_key,
                         const GURL& script_server_url,
                         const GURL& action_server_url,
                         content::BrowserContext* context,
                         std::unique_ptr<ClientContext> client_context,
                         AccessTokenFetcher* access_token_fetcher,
                         bool auth_enabled)
    : context_(context),
      script_server_url_(script_server_url),
      script_action_server_url_(action_server_url),
      api_key_(api_key),
      client_context_(std::move(client_context)),
      access_token_fetcher_(access_token_fetcher),
      auth_enabled_(auth_enabled) {
  DCHECK(script_server_url.is_valid());
  DCHECK(action_server_url.is_valid());
}

ServiceImpl::~ServiceImpl() {}

void ServiceImpl::GetScriptsForUrl(const GURL& url,
                                   const TriggerContext& trigger_context,
                                   ResponseCallback callback) {
  DCHECK(url.is_valid());

  client_context_->Update(trigger_context);
  SendRequest(AddLoader(
      script_server_url_,
      ProtocolUtils::CreateGetScriptsRequest(url, client_context_->AsProto(),
                                             trigger_context.GetParameters()),
      std::move(callback)));
}

bool ServiceImpl::IsLiteService() const {
  return false;
}

void ServiceImpl::GetActions(const std::string& script_path,
                             const GURL& url,
                             const TriggerContext& trigger_context,
                             const std::string& global_payload,
                             const std::string& script_payload,
                             ResponseCallback callback) {
  DCHECK(!script_path.empty());

  client_context_->Update(trigger_context);
  SendRequest(AddLoader(
      script_action_server_url_,
      ProtocolUtils::CreateInitialScriptActionsRequest(
          script_path, url, global_payload, script_payload,
          client_context_->AsProto(), trigger_context.GetParameters()),
      std::move(callback)));
}

void ServiceImpl::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    ResponseCallback callback) {
  client_context_->Update(trigger_context);
  SendRequest(AddLoader(script_action_server_url_,
                        ProtocolUtils::CreateNextScriptActionsRequest(
                            previous_global_payload, previous_script_payload,
                            processed_actions, client_context_->AsProto()),
                        std::move(callback)));
}

void ServiceImpl::SendRequest(Loader* loader) {
  if (access_token_.empty() && auth_enabled_) {
    // Trigger a fetch of the access token. All loaders in loaders_ will be
    // started later on, the access token is available.
    FetchAccessToken();
    return;
  }

  StartLoader(loader);
}

ServiceImpl::Loader::Loader() : retried_with_fresh_access_token(false) {}
ServiceImpl::Loader::~Loader() {}

ServiceImpl::Loader* ServiceImpl::AddLoader(const GURL& url,
                                            const std::string& request_body,
                                            ResponseCallback callback) {
  std::unique_ptr<Loader> loader = std::make_unique<Loader>();
  loader->url = url;
  loader->request_body = request_body;
  loader->callback = std::move(callback);
  Loader* loader_ptr = loader.get();
  loaders_[loader_ptr] = std::move(loader);
  return loader_ptr;
}

void ServiceImpl::StartLoader(Loader* loader) {
  if (loader->loader)
    return;

  auto resource_request = std::make_unique<::network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->redirect_mode = ::network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = ::network::mojom::CredentialsMode::kOmit;
  if (access_token_.empty()) {
    std::string query_str = base::StrCat({"key=", api_key_});
    // query_str must remain valid until ReplaceComponents() has returned.
    url::StringPieceReplacements<std::string> add_key;
    add_key.SetQueryStr(query_str);
    resource_request->url = loader->url.ReplaceComponents(add_key);
  } else {
    resource_request->url = loader->url;
    resource_request->headers.SetHeader(
        "Authorization", base::StrCat({"Bearer ", access_token_}));
  }

  loader->loader = ::network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  loader->loader->AttachStringForUpload(loader->request_body,
                                        "application/x-protobuffer");
#ifdef DEBUG
  loader->loader->SetAllowHttpErrorResults(true);
#endif
  loader->loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      content::BrowserContext::GetDefaultStoragePartition(context_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&ServiceImpl::OnURLLoaderComplete, base::Unretained(this),
                     loader));
}

void ServiceImpl::OnURLLoaderComplete(
    Loader* loader,
    std::unique_ptr<std::string> response_body) {
  auto loader_it = loaders_.find(loader);
  DCHECK(loader_it != loaders_.end());

  int response_code = 0;
  if (loader->loader->ResponseInfo() &&
      loader->loader->ResponseInfo()->headers) {
    response_code = loader->loader->ResponseInfo()->headers->response_code();
  }

  // When getting a 401, refresh the auth token - but only try this once.
  if (response_code == 401 && auth_enabled_ && !access_token_.empty() &&
      !loader->retried_with_fresh_access_token) {
    loader->retried_with_fresh_access_token = true;
    loader->loader.reset();
    // Invalidate access token and load a new one.
    access_token_fetcher_->InvalidateAccessToken(access_token_);
    access_token_.clear();
    SendRequest(loader);
    return;
  }

  // Take ownership of loader.
  std::unique_ptr<Loader> loader_instance = std::move(loader_it->second);
  loaders_.erase(loader_it);
  DCHECK(loader_instance);

  std::string response_body_str;
  if (loader_instance->loader->NetError() != net::OK || response_code != 200) {
    LOG(ERROR) << "Communicating with autofill assistant server error NetError="
               << loader_instance->loader->NetError()
               << " response_code=" << response_code << " message="
               << (response_body == nullptr ? "" : *response_body);
    // TODO(crbug.com/806868): Pass an enum to be able to distinguish errors
    // downstream. Also introduce a metric for this.
    std::move(loader_instance->callback).Run(false, response_body_str);
    return;
  }

  if (response_body)
    response_body_str = std::move(*response_body);
  std::move(loader_instance->callback).Run(true, response_body_str);
}

void ServiceImpl::FetchAccessToken() {
  if (fetching_token_)
    return;

  fetching_token_ = true;
  access_token_fetcher_->FetchAccessToken(base::BindOnce(
      &ServiceImpl::OnFetchAccessToken, weak_ptr_factory_.GetWeakPtr()));
}

void ServiceImpl::OnFetchAccessToken(bool success,
                                     const std::string& access_token) {
  fetching_token_ = false;

  if (!success) {
    auth_enabled_ = false;
    // Give up on authentication for this run. Let the pending requests through,
    // which might be rejected, depending on the server configuration.
    return;
  }

  access_token_ = access_token;

  // Start any pending requests with the access token.
  for (const auto& entry : loaders_) {
    StartLoader(entry.first);
  }
}

}  // namespace autofill_assistant
