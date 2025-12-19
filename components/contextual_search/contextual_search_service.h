// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SERVICE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SERVICE_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefRegistrySimple;
class TemplateURLService;

namespace signin {
class IdentityManager;
}

namespace variations {
class VariationsClient;
}

namespace contextual_search {
class ContextualSearchSessionEntry;

// Manages the lifecycle of ComposeboxQueryController instances for a Profile.
class ContextualSearchService : public KeyedService {
 public:
  using SessionId = base::UnguessableToken;
  class SessionHandle;

  ContextualSearchService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      version_info::Channel channel,
      const std::string& locale);
  ~ContextualSearchService() override;

  // Register profile related prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Check whether contextual search is enabled.
  static bool IsContextSharingEnabled(const PrefService* prefs);

  // Creates a new session and returns a handle to it.
  std::unique_ptr<ContextualSearchSessionHandle> CreateSession(
      std::unique_ptr<ContextualSearchContextController::ConfigParams>
          query_controller_config_params,
      ContextualSearchSource source);
  // Returns a new handle for an existing session. Returns nullptr if the
  // session does not exist (e.g. has been released).
  std::unique_ptr<ContextualSearchSessionHandle> GetSession(
      const SessionId& session_id);

  std::unique_ptr<ContextualSearchSessionHandle> CreateSessionForTesting(
      std::unique_ptr<ContextualSearchContextController> controller,
      std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder);

 protected:
  friend class ContextualSearchSessionHandle;

  // Creates and returns a ContextualSearchContextController.
  virtual std::unique_ptr<ContextualSearchContextController>
  CreateComposeboxQueryController(
      std::unique_ptr<ContextualSearchContextController::ConfigParams>
          query_controller_config_params);

  // Called by SessionHandle to retrieve a reference to the session controller.
  ContextualSearchContextController* GetSessionController(
      const SessionId& session_id);

  // Called by SessionHandle to retrieve a reference to the metrics recorder.
  ContextualSearchMetricsRecorder* GetSessionMetricsRecorder(
      const SessionId& session_id);

  // Called by SessionHandle to manage ref counts.
  void ReleaseSession(const SessionId& session_id);

  // KeyedService:
  void Shutdown() override;

  // Map of active sessions, keyed by the session ID.
  std::map<SessionId, ContextualSearchSessionEntry> sessions_;

  raw_ptr<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<TemplateURLService> template_url_service_;
  const raw_ptr<variations::VariationsClient> variations_client_;
  const version_info::Channel channel_;
  const std::string locale_;

  base::WeakPtrFactory<ContextualSearchService> weak_ptr_factory_{this};
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SERVICE_H_
