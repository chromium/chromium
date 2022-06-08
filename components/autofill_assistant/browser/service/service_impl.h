// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client_context.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace autofill_assistant {
class Client;

// Native autofill assistant service which communicates with the server to get
// scripts and client actions.
// TODO(b/158998456): Add unit tests.
class ServiceImpl : public Service {
 public:
  // Convenience method for creating a service. |context| and |client| must
  // remain valid for the lifetime of the service instance. Will enable
  // authentication unless disabled via the autofill-assistant-auth command line
  // flag.
  static std::unique_ptr<ServiceImpl> Create(content::BrowserContext* context,
                                             Client* client);
  // Same as above, but allows injecting a specific endpoint.
  static std::unique_ptr<ServiceImpl> Create(
      content::BrowserContext* context,
      Client* client,
      const ServerUrlFetcher& url_fetcher);

  ServiceImpl(Client* client,
              std::unique_ptr<ServiceRequestSender> request_sender,
              const GURL& script_server_url,
              const GURL& action_server_url,
              const GURL& user_data_url,
              std::unique_ptr<ClientContext> client_context);
  ServiceImpl(const ServiceImpl&) = delete;
  ServiceImpl& operator=(const ServiceImpl&) = delete;
  ~ServiceImpl() override;

  // Get scripts for a given |url|, which should be a valid URL.
  void GetScriptsForUrl(
      const GURL& url,
      const TriggerContext& trigger_context,
      ServiceRequestSender::ResponseCallback callback) override;

  // Get actions.
  void GetActions(const std::string& script_path,
                  const GURL& url,
                  const TriggerContext& trigger_context,
                  const std::string& global_payload,
                  const std::string& script_payload,
                  ServiceRequestSender::ResponseCallback callback) override;

  // Get next sequence of actions according to server payloads in previous
  // response.
  void GetNextActions(
      const TriggerContext& trigger_context,
      const std::string& previous_global_payload,
      const std::string& previous_script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      const RoundtripTimingStats& timing_stats,
      const RoundtripNetworkStats& network_stats,
      ServiceRequestSender::ResponseCallback callback) override;

  void SetScriptStoreConfig(
      const ScriptStoreConfig& script_store_config) override;

  void GetUserData(const CollectUserDataOptions& options,
                   uint64_t run_id,
                   const UserData* user_data,
                   ServiceRequestSender::ResponseCallback callback) override;

  void SetDisableRpcSigning(bool disable_rpc_signing) override;

  void UpdateAnnotateDomModelContext(int64_t model_version) override;

  void UpdateJsFlowLibraryLoaded(bool js_flow_library_loaded) override;

 private:
  void SendUserDataRequest(
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
      const std::string& client_token);

  const raw_ptr<Client> client_;

  // The request sender responsible for communicating with a remote endpoint.
  std::unique_ptr<ServiceRequestSender> request_sender_;

  // The RPC endpoints to send requests to.
  GURL script_server_url_;
  GURL script_action_server_url_;
  GURL user_data_url_;

  // The client context to send to the backend.
  std::unique_ptr<ClientContext> client_context_;

  // The script store config used for GetActions request. This is set by the
  // controller, obtained from the GetScriptsForUrl's response.
  absl::optional<ScriptStoreConfig> script_store_config_;

  base::WeakPtrFactory<ServiceImpl> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_IMPL_H_
