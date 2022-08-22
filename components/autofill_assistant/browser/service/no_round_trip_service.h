// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_NO_ROUND_TRIP_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_NO_ROUND_TRIP_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "url/gurl.h"

namespace autofill_assistant {

struct LocalScriptStore {
  // Temporary boilerplate, this struct becomes a proper class in the child CL
  ~LocalScriptStore();
  LocalScriptStore(const LocalScriptStore&);
  LocalScriptStore(LocalScriptStore&&);
  LocalScriptStore& operator=(const LocalScriptStore&) = default;
  LocalScriptStore& operator=(LocalScriptStore&&) = default;
  LocalScriptStore(std::vector<GetNoRoundtripScriptsByHashPrefixResponseProto::
                                   MatchInfo::RoutineScript> routines,
                   std::string domain,
                   SupportsScriptResponseProto supports_site_response);

  // Contains pairs of [AutobotResponses, SupportedRoutine].
  // TODO change SupportedRoutine to simply script_path
  std::vector<
      GetNoRoundtripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript>
      routines;

  // The domain that this LocalScriptStore is valid for
  std::string domain;

  // The results of SupportsScript for this domain/intent match
  SupportsScriptResponseProto supports_site_response;
};

// An offline version of the service that fetches all actions at once and then
// serves scripts without roundtrips.
class NoRoundTripService : public Service {
 public:
  // Convenience method for creating an offline service.
  static std::unique_ptr<NoRoundTripService> Create(
      const LocalScriptStore& script_store);

  explicit NoRoundTripService(const LocalScriptStore& script_store);
  NoRoundTripService(const NoRoundTripService&) = delete;
  NoRoundTripService& operator=(const NoRoundTripService&) = delete;
  ~NoRoundTripService() override;

  // Return the SupportsSite response from the local script store.
  void GetScriptsForUrl(
      const GURL& url,
      const TriggerContext& trigger_context,
      ServiceRequestSender::ResponseCallback callback) override;

  // Return the action given a script_path from the local script store.
  void GetActions(const std::string& script_path,
                  const GURL& url,
                  const TriggerContext& trigger_context,
                  const std::string& global_payload,
                  const std::string& script_payload,
                  ServiceRequestSender::ResponseCallback callback) override;

  // This call will not work with the local script store.
  void GetNextActions(
      const TriggerContext& trigger_context,
      const std::string& previous_global_payload,
      const std::string& previous_script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      const RoundtripTimingStats& timing_stats,
      const RoundtripNetworkStats& network_stats,
      ServiceRequestSender::ResponseCallback callback) override;

  // This call will return the script store config from the local script store.
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

 private:
  ScriptStoreConfig script_store_config_;
  const LocalScriptStore script_store_;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_NO_ROUND_TRIP_SERVICE_H_
