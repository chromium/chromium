// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_FEDERATED_PERMISSION_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_FEDERATED_PERMISSION_H_

#include <string>
#include <vector>

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "url/origin.h"

namespace actor_login {

// Represents a permission to use a federated (FedCM) credential.
struct FederatedPermission {
  FederatedPermission();
  FederatedPermission(const FederatedPermission&);
  FederatedPermission& operator=(const FederatedPermission&);
  ~FederatedPermission();

  bool MatchesFederatedCredential(const Credential& credential) const;

  // IdP origin that identifies the identity provider. For example,
  // "https://accounts.google.com".
  url::Origin idp_origin;
  // Origin of the main frame of the website where actor login was initiated.
  url::Origin rp_embedder_origin;
  // Origin of the iframe that initiated the FedCM flow.
  url::Origin rp_requester_origin;
  // Account ID of the account used for actor login.
  std::string chosen_account_id;
  // Email of the account used for actor login.
  std::string chosen_account_email;
  // Output only. Lists origins that are affiliated with the requester origin.
  std::vector<std::string> affiliated_requester_origins;

#if defined(UNIT_TEST)
  friend bool operator==(const FederatedPermission&,
                         const FederatedPermission&) = default;
#endif
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_FEDERATED_PERMISSION_H_
