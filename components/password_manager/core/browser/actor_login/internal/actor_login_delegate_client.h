// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class PrefService;

namespace translate {
class TranslateManager;
}

namespace password_manager {
class PasswordManagerClient;
class PasswordManagerDriver;
}

namespace actor_login {

class ActionSequenceDelegate;
class ActorLoginCredentialsFetcher;
class ActorLoginMetricsHelper;
class ActorLoginPermissionCleaningService;
class ActorLoginQualityLoggerInterface;
class ActorLoginSiwgControllerInterface;
class ActorLoginWebContentInterface;

// Client interface for `ActorLoginDelegate`.
class ActorLoginDelegateClient {
 public:
  ActorLoginDelegateClient() = default;
  virtual ~ActorLoginDelegateClient() = default;

  // Not copyable or movable.
  ActorLoginDelegateClient(const ActorLoginDelegateClient&) = delete;
  ActorLoginDelegateClient& operator=(const ActorLoginDelegateClient&) = delete;

  // Registers the `ActorLoginWebContentInterface` interface to receive web
  // events.
  virtual void SetActorLoginWebContentInterface(
      ActorLoginWebContentInterface* web_interface) = 0;

  // Returns the preference service associated with the profile.
  virtual PrefService* GetPrefs() = 0;

  // Returns the password manager client. This might return `nullptr` if the
  // user is using a third-party password manager on Android.
  virtual password_manager::PasswordManagerClient*
  GetPasswordManagerClient() = 0;

  // Returns the password manager driver associated with the primary main frame
  // of the page.
  virtual password_manager::PasswordManagerDriver*
  GetPasswordManagerDriverForMainFrame() = 0;

  // Returns the UKM source ID associated with the primary main frame of the
  // page.
  virtual ukm::SourceId GetPageUkmSourceIdForMainFrame() = 0;

  // Returns the last committed origin of the primary main frame of the page.
  virtual url::Origin GetLastCommittedOriginForMainFrame() = 0;

  // Returns the translate manager associated with the profile.
  virtual translate::TranslateManager* GetTranslateManager() = 0;

  // Returns the permission cleaning service associated with the profile.
  virtual ActorLoginPermissionCleaningService*
  GetPermissionCleaningService() = 0;

  // Creates a federated credentials fetcher.
  virtual std::unique_ptr<ActorLoginCredentialsFetcher>
  CreateFederatedCredentialsFetcher(
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      ActorLoginMetricsHelper* metrics_helper) = 0;

  // Creates a controller for Sign-in with Google interaction.
  virtual std::unique_ptr<ActorLoginSiwgControllerInterface>
  CreateSiwgController(
      const Credential& credential,
      bool should_store_permission,
      LoginStatusResultOrErrorReply on_finished_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      base::OnceCallback<void(bool)>
          post_button_click_login_result_callback) = 0;

  // Checks whether the currently ongoing task is in focus, either in the tab or
  // in its corresponding Glic UI instance.
  virtual bool IsTaskInFocus() = 0;

  // Returns whether FedCm embedder-initiated login is supported.
  virtual bool SupportsFedCmEmbedderInitiatedLogin() = 0;

  // Removes the federated embedder login request.
  virtual void RemoveFederatedEmbedderLoginRequest() = 0;

  // Starts observing the ongoing actor task's control state.
  virtual void ObserveControlStateForCurrentTask(
      base::OnceClosure on_released_callback) = 0;

  virtual base::WeakPtr<ActorLoginDelegateClient> AsWeakPtr() = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_CLIENT_H_
