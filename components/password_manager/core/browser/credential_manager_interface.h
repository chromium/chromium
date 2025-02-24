// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_INTERFACE_H_

#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/prefs/pref_member.h"

namespace password_manager {

using StoreCallback = base::OnceCallback<void()>;
using PreventSilentAccessCallback = base::OnceCallback<void()>;
using GetCallback =
    base::OnceCallback<void(CredentialManagerError,
                            const std::optional<CredentialInfo>&)>;

// Interface for classes implementing Credential Manager methods Store,
// PreventSilentAccess and Get. Each method takes a callback as an
// argument and runs the callback with the result. Platform specific code
// and UI invocations are performed by the interface implementations.
class CredentialManagerInterface {
 public:
  virtual ~CredentialManagerInterface() = default;

  // Stores a `credential` for later retrieval.
  // The `callback` should be executed to send back an acknowledge response.
  virtual void Store(const CredentialInfo& credential,
                     StoreCallback callback) = 0;

  // Sets a flag that specifies whether automatic log in is allowed for future
  // visits to the current origin.
  // The `callback` should be executed to send back an acknowledge response.
  virtual void PreventSilentAccess(PreventSilentAccessCallback callback) = 0;

  // Gets a credential that can be used to authenticate a user on a website.
  // The `mediation` argument indicates how and whether the user should be asked
  // to participate in the operation.
  // The `requested_credential_type_flags` indicates which types of credentials
  // are being requested.
  // The `federations` argument decides from which origins the credentials are
  // being requested.
  // The `callback` should be executed with the single credential that will be
  // used to authenticate or with an error.
  virtual void Get(CredentialMediationRequirement mediation,
                   int requested_credential_type_flags,
                   const std::vector<GURL>& federations,
                   GetCallback callback) = 0;

  virtual void ResetPendingRequest() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_INTERFACE_H_
