// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_INTERFACE_H_

#include "base/memory/ref_counted.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/translate/core/browser/translate_manager.h"

namespace actor_login {

// Manages Model Logging Quality and uploads logs to the server.
// Each log corresponds to a single filling, which means there would be
// at most one GetCredentials request, and at most two AttemptLogin requests.
// The second AttemptLogin request can happen if the first one failed with
// kErrorDeviceReauthRequired.
//
// The logger is shared by every participant of a single login flow (the tool
// which starts the flow, the credential fetchers, the credential filler and
// the Sign-in-with-Google controller). These participants have different
// lifetimes: in particular, the Sign-in-with-Google controller outlives the
// tool whenever the login requires a button click, and it reports the outcome
// of the login only after the click happened. The log is therefore uploaded
// once the last reference is dropped, i.e. when every participant of the flow
// is done with it.
//
// The reference count is not atomic, so every participant must hold and drop
// its reference on the same sequence, which is the UI thread.
class ActorLoginQualityLoggerInterface
    : public base::RefCounted<ActorLoginQualityLoggerInterface> {
 public:
  virtual void SetDomainAndLanguage(
      translate::TranslateManager* translate_manager,
      const GURL& url) = 0;
  virtual void SetGetCredentialsDetails(
      optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails
          get_credentials_details) = 0;

  virtual void SetFederatedGetCredentialsDetails(
      optimization_guide::proto::
          ActorLoginQuality_FederatedGetCredentialsDetails
              federated_get_credentials_details) = 0;

  virtual void AddAttemptLoginDetails(
      optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
          attempt_login_details) = 0;

  virtual void SetPermissionPicked(
      optimization_guide::proto::ActorLoginQuality_PermissionOption
          permission_option) = 0;

 protected:
  friend class base::RefCounted<ActorLoginQualityLoggerInterface>;

  // Implementations upload the final log from their destructor, which runs
  // once the last participant of the login flow drops its reference.
  virtual ~ActorLoginQualityLoggerInterface() = default;
};
}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_QUALITY_LOGGER_INTERFACE_H_
