// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_NO_ROUND_TRIP_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_NO_ROUND_TRIP_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/local_script_store.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace autofill_assistant {

// An offline version of the service that fetches all actions at once and then
// serves scripts without roundtrips.
class NoRoundTripService : public Service {
 public:
  // Factory methods to create a NoRoundTrip service, all endpoint are
  // initialised, but no RPC is made.
  [[nodiscard]] static std::unique_ptr<NoRoundTripService> Create(
      content::BrowserContext* context,
      Client* client);
  [[nodiscard]] static std::unique_ptr<NoRoundTripService> Create(
      content::BrowserContext* context,
      Client* client,
      const ServerUrlFetcher& url_fetcher);

  // Constructs a NoRoundTripService, does not make any call to the server, the
  // calls are instead made in the first call to GetScripts.
  NoRoundTripService(std::unique_ptr<ServiceRequestSender> request_sender,
                     const GURL& get_scripts_endpoint,
                     Client* client);

  // Initializes a NoRoundTripService, only used for testing purposes.
  // Does not initializes RPC endpoints and the client.
  explicit NoRoundTripService(std::unique_ptr<LocalScriptStore> script_store);

  NoRoundTripService(const NoRoundTripService&) = delete;
  NoRoundTripService& operator=(const NoRoundTripService&) = delete;
  ~NoRoundTripService() override;

  // Runs an RPC to GetNoRoundTripScriptsByHash, stores the results in the local
  // store and finally calls the callback with a fabricated GetActionsResponse
  // created from the local script store.
  void GetScriptsForUrl(
      const GURL& url,
      const TriggerContext& trigger_context,
      ServiceRequestSender::ResponseCallback callback) override;

  // Calls the callback with a fabricated GetActionsResponse created given a
  // script_path from the local script store.
  void GetActions(const std::string& script_path,
                  const GURL& url,
                  const TriggerContext& trigger_context,
                  const std::string& global_payload,
                  const std::string& script_payload,
                  ServiceRequestSender::ResponseCallback callback) override;

  // This call will always call the callback with an empty response.
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

  // This call will not work with the local script store.
  void GetUserData(const CollectUserDataOptions& options,
                   uint64_t run_id,
                   const UserData* user_data,
                   ServiceRequestSender::ResponseCallback callback) override;

  // This call will not work with the local script store.
  void SetDisableRpcSigning(bool disable_rpc_signing) override;

  // This call will not work with the local script store.
  void UpdateAnnotateDomModelContext(int64_t model_version) override;

  // This call will not work with the local script store.
  void UpdateJsFlowLibraryLoaded(bool js_flow_library_loaded) override;

  // TODO(mruel): keeping the implementation of this function for a child CL so
  // it can be reviewed separately.
  void ReportProgress(const std::string& token,
                      const std::string& payload,
                      ServiceRequestSender::ResponseCallback callback) override;

  // Retrieves the script store, used for testing.
  const LocalScriptStore* GetStore() const;

 private:
  // Callback to run on receiving a response from
  // OnNoRoundTripByHashPrefixRequest
  void OnNoRountripByHashPrefixResponse(
      const GURL& url,
      ServiceRequestSender::ResponseCallback callback,
      int http_status,
      const std::string& response,
      const ServiceRequestSender::ResponseInfo& response_info);

  // Create a GetNoRoundtrip request given the url and the script parameters.
  [[nodiscard]] static std::string CreateGetNoRoundtripRequest(
      const GURL& url,
      const ScriptParameters& script_parameters);

  // RPC endpoint
  GURL get_scripts_endpoint_;

  raw_ptr<Client> client_;

  ScriptStoreConfig script_store_config_;

  std::unique_ptr<LocalScriptStore> script_store_;

  std::unique_ptr<ServiceRequestSender> request_sender_;

  base::WeakPtrFactory<NoRoundTripService> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_NO_ROUND_TRIP_SERVICE_H_
