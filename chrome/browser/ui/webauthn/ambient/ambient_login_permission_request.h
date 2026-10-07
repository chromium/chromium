// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/permissions/permission_request.h"
#include "url/gurl.h"

namespace ambient_signin {

enum class CredentialType {
  kPasskey,
  kPassword,
};

// Represents a WebAuthn passkey or saved password credential option.
struct PasskeyOrPasswordCredential {
  std::u16string username;
  std::u16string provider_name;
  CredentialType type = CredentialType::kPasskey;
  std::u16string display_name;
};

// Represents a federated sign-in credential option (e.g. from FedCM).
struct FederatedCredential {
  std::u16string idp_name;
  std::u16string account_name;
  std::u16string email;
  GURL idp_origin;
};

// A permission request for displaying ambient login options (e.g. WebAuthn
// passkeys/passwords or FedCM sign-in) via PermissionRequestManager.
class AmbientLoginPermissionRequest : public permissions::PermissionRequest {
 public:
  using PasskeyOrPasswordSelectedCallback =
      base::OnceCallback<void(size_t index)>;
  using FederatedCredentialSelectedCallback =
      base::OnceCallback<void(size_t index)>;

  // Constructor supporting both WebAuthn/password credentials and FedCM
  // federated credentials.
  AmbientLoginPermissionRequest(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      std::vector<PasskeyOrPasswordCredential> credentials,
      PasskeyOrPasswordSelectedCallback credential_selected_callback,
      std::vector<FederatedCredential> federated_credentials,
      FederatedCredentialSelectedCallback federated_selected_callback,
      PermissionDecidedCallback permission_decided_callback =
          base::DoNothing());

  // Convenience constructor for passkey/password-only requests.
  AmbientLoginPermissionRequest(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      std::vector<PasskeyOrPasswordCredential> credentials,
      PasskeyOrPasswordSelectedCallback credential_selected_callback,
      PermissionDecidedCallback permission_decided_callback =
          base::DoNothing());

  ~AmbientLoginPermissionRequest() override;

  const std::vector<PasskeyOrPasswordCredential>& credentials() const {
    return credentials_;
  }
  const std::vector<FederatedCredential>& federated_credentials() const {
    return federated_credentials_;
  }

  // Records the chosen credential index to be passed to
  // `credential_selected_callback_` when the permission request is accepted.
  void SelectCredential(size_t index);

  // Records the chosen federated credential index to be passed to
  // `federated_selected_callback_` when the permission request is accepted.
  void SelectFederatedCredential(size_t index);

  // permissions::PermissionRequest:
  std::u16string GetMessageTextFragment() const override;

 private:
  void OnPermissionDecided(
      const permissions::PermissionPromptDecision& decision,
      const permissions::PermissionRequestData& request_data);

  std::vector<PasskeyOrPasswordCredential> credentials_;
  PasskeyOrPasswordSelectedCallback credential_selected_callback_;
  std::vector<FederatedCredential> federated_credentials_;
  FederatedCredentialSelectedCallback federated_selected_callback_;
  PermissionDecidedCallback caller_permission_decided_callback_;
  std::optional<size_t> selected_credential_index_;
  std::optional<size_t> selected_federated_index_;
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_LOGIN_PERMISSION_REQUEST_H_
