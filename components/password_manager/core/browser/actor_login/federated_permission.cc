// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/federated_permission.h"

namespace actor_login {

FederatedPermission::FederatedPermission() = default;
FederatedPermission::FederatedPermission(const FederatedPermission&) = default;
FederatedPermission& FederatedPermission::operator=(
    const FederatedPermission&) = default;
FederatedPermission::~FederatedPermission() = default;

bool FederatedPermission::MatchesFederatedCredential(
    const Credential& credential) const {
  CHECK(credential.type == CredentialType::kFederated);
  return credential.federation_detail->account_id == chosen_account_id &&
         credential.request_origin == rp_embedder_origin;
}

}  // namespace actor_login
