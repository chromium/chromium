// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_CREDENTIAL_MANAGER_INTERFACE_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_CREDENTIAL_MANAGER_INTERFACE_H_

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace password_manager {
enum class CredentialManagerError;
struct CredentialInfo;
enum class CredentialMediationRequirement;
}  // namespace password_manager

namespace credential_management {

using StoreCallback = base::OnceCallback<void()>;
using PreventSilentAccessCallback = base::OnceCallback<void()>;
using GetCallback = base::OnceCallback<void(
    password_manager::CredentialManagerError,
    const std::optional<password_manager::CredentialInfo>&)>;

// Interface for classes implementing Credential Manager methods Store,
// PreventSilentAccess and Get. Each method takes a callback as an
// argument and runs the callback with the result. Platform specific code
// and UI invocations are performed by the interface implementations.
class CredentialManagerInterface {
 public:
  virtual ~CredentialManagerInterface() = default;

  // Stores a `credential` for later retrieval.
  // The `callback` should be executed to send back an acknowledge response.
  virtual void Store(const password_manager::CredentialInfo& credential,
                     StoreCallback callback) = 0;

  // Sets a flag that specifies whether automatic log in is allowed for future
  // visits to the current origin.
  // The `callback` should be executed to send back an acknowledge response.
  virtual void PreventSilentAccess(PreventSilentAccessCallback callback) = 0;

  // Gets a credential that can be used to authenticate a user on a website.
  // The `mediation` argument indicates how and whether the user should be asked
  // to participate in the operation.
  // The `include_passwords` indicates whether passwords are being requested.
  // The `federations` argument decides from which origins the credentials are
  // being requested.
  // The `callback` should be executed with the single credential that will be
  // used to authenticate or with an error.
  virtual void Get(password_manager::CredentialMediationRequirement mediation,
                   bool include_passwords,
                   const std::vector<GURL>& federations,
                   GetCallback callback) = 0;

  // `ContentCredentialManager` that uses the `CredentialManagerInterface`
  // implementation will call this method in `DisconnectBinding`.
  // That happens when the `ContentCredentialManager` needs to service API calls
  // in the context of the new WebContents::GetLastCommittedURL so the
  // `CredentialManagerInterface` implementation needs to reset for that new
  // URL.
  virtual void ResetAfterDisconnecting() = 0;
};

}  // namespace credential_management

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_CREDENTIAL_MANAGER_INTERFACE_H_
