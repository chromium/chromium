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
#include "components/keyed_service/core/keyed_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class TemplateURLService;

namespace signin {
class IdentityManager;
}

namespace variations {
class VariationsClient;
}

namespace contextual_search {

// Manages the lifecycle of ComposeboxQueryController instances for a Profile.
class ContextualSearchService : public KeyedService {
 public:
  using SessionId = base::UnguessableToken;
  class SessionHandle;

  static inline constexpr char kDefaultRecorderName[] =
      "UnnamedMetricsRecorder";

  ContextualSearchService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      version_info::Channel channel,
      const std::string& locale);
  ~ContextualSearchService() override;

  // Creates a new session and returns a handle to it.
  std::unique_ptr<SessionHandle> CreateSession(
      std::unique_ptr<ContextualSearchContextController::ConfigParams>
          query_controller_config_params,
      const std::string& composebox_metric_name = kDefaultRecorderName);
  // Returns a new handle for an existing session. Returns nullptr if the
  // session does not exist (e.g. has been released).
  std::unique_ptr<SessionHandle> GetSession(const SessionId& session_id);

  std::unique_ptr<SessionHandle> CreateSessionForTesting(
      std::unique_ptr<ContextualSearchContextController> controller,
      std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder);

 protected:
  friend class SessionHandle;
  class SessionEntry;

  // Called by SessionHandle to retrieve a reference to the session controller.
  ContextualSearchContextController* GetSessionController(
      const SessionId& session_id);

  // Called by SessionHandle to retrieve a reference to the metrics recorder.
  ContextualSearchMetricsRecorder* GetSessionMetricsRecorder(
      const SessionId& session_id);

  // Called by SessionHandle to retrieve name of metrics recorder.
  std::string GetSessionMetricsRecorderName(const SessionId& session_id) const;

  // Called by SessionHandle to manage ref counts.
  void ReleaseSession(const SessionId& session_id);

  // Map of active sessions, keyed by the session ID.
  std::map<SessionId, SessionEntry> sessions_;

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<TemplateURLService> template_url_service_;
  const raw_ptr<variations::VariationsClient> variations_client_;
  const version_info::Channel channel_;
  const std::string locale_;

  base::WeakPtrFactory<ContextualSearchService> weak_ptr_factory_{this};
};

// RAII handle for managing the lifetime of a ComposeboxQueryController.
class ContextualSearchService::SessionHandle {
 public:
  SessionHandle(const SessionHandle&) = delete;
  SessionHandle& operator=(const SessionHandle&) = delete;
  SessionHandle(SessionHandle&&) = delete;
  SessionHandle& operator=(SessionHandle&&) = delete;
  ~SessionHandle();

  base::UnguessableToken session_id() const { return session_id_; }
  // Returns the ContextualSearchContextController reference held by this
  // handle or nullptr if the session is not valid.
  ContextualSearchContextController* GetController() const;

  // Returns the ContextualSearchMetricsRecorder reference held by this handle
  // or nullptr if the session is not valid.
  ContextualSearchMetricsRecorder* GetMetricsRecorder() const;

  std::string GetMetricsRecorderName() const;

 private:
  friend class ContextualSearchService;

  SessionHandle(base::WeakPtr<ContextualSearchService> service,
                const SessionId& session_id);

  // The service that vended this handle. This is a weak pointer because a
  // handle may outlive the service.
  const base::WeakPtr<ContextualSearchService> service_;
  const base::UnguessableToken session_id_;
};

// An entry in the session map, containing the ComposeboxQueryController and its
// reference count.
class ContextualSearchService::SessionEntry {
 public:
  SessionEntry(const SessionEntry&) = delete;
  SessionEntry& operator=(const SessionEntry&) = delete;
  SessionEntry(SessionEntry&&);
  SessionEntry& operator=(SessionEntry&&);
  ~SessionEntry();

 private:
  friend class ContextualSearchService;

  explicit SessionEntry(
      std::unique_ptr<ContextualSearchContextController> controller,
      std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder);

  std::unique_ptr<ContextualSearchContextController> controller_;
  std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder_;

  size_t ref_count_ = 1;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SERVICE_H_
