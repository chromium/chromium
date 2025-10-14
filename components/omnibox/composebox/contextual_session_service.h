// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SESSION_SERVICE_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SESSION_SERVICE_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class ComposeboxQueryController;
class TemplateURLService;

namespace signin {
class IdentityManager;
}

namespace variations {
class VariationsClient;
}

// Manages the lifecycle of ComposeboxQueryController instances for a Profile.
class ContextualSessionService : public KeyedService {
 public:
  using SessionId = base::UnguessableToken;
  class SessionHandle;

  ContextualSessionService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      version_info::Channel channel,
      const std::string& locale);
  ~ContextualSessionService() override;

  // Creates a new session and returns a handle to it.
  std::unique_ptr<SessionHandle> CreateSession(
      std::unique_ptr<ComposeboxQueryController::QueryControllerConfigParams>
          query_controller_config_params);
  // Returns a new handle for an existing session. Returns nullptr if the
  // session does not exist (e.g. has been released).
  std::unique_ptr<SessionHandle> GetSession(const SessionId& session_id);

  std::unique_ptr<SessionHandle> CreateSessionForTesting(
      std::unique_ptr<ComposeboxQueryController> controller);

 protected:
  friend class SessionHandle;
  class SessionEntry;

  // Called by SessionHandle to retrieve a reference to the session controller.
  ComposeboxQueryController* GetSessionController(const SessionId& session_id);
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

  base::WeakPtrFactory<ContextualSessionService> weak_ptr_factory_{this};
};

// RAII handle for managing the lifetime of a ComposeboxQueryController.
class ContextualSessionService::SessionHandle {
 public:
  SessionHandle(const SessionHandle&) = delete;
  SessionHandle& operator=(const SessionHandle&) = delete;
  SessionHandle(SessionHandle&&) = delete;
  SessionHandle& operator=(SessionHandle&&) = delete;
  ~SessionHandle();

  base::UnguessableToken session_id() const { return session_id_; }
  // Returns the ComposeboxQueryController reference held by this handle or
  // nullptr if the session is not valid.
  ComposeboxQueryController* GetController() const;

 private:
  friend class ContextualSessionService;

  SessionHandle(base::WeakPtr<ContextualSessionService> service,
                const SessionId& session_id);

  // The service that vended this handle. This is a weak pointer because a
  // handle may outlive the service.
  const base::WeakPtr<ContextualSessionService> service_;
  const base::UnguessableToken session_id_;
};

// An entry in the session map, containing the ComposeboxQueryController and its
// reference count.
class ContextualSessionService::SessionEntry {
 public:
  SessionEntry(const SessionEntry&) = delete;
  SessionEntry& operator=(const SessionEntry&) = delete;
  SessionEntry(SessionEntry&&);
  SessionEntry& operator=(SessionEntry&&);
  ~SessionEntry();

 private:
  friend class ContextualSessionService;

  explicit SessionEntry(std::unique_ptr<ComposeboxQueryController> controller);

  std::unique_ptr<ComposeboxQueryController> controller_;
  size_t ref_count_ = 1;
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SESSION_SERVICE_H_
