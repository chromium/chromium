// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_FRONTEND_TRUSTED_VAULT_CONNECTION_H_
#define COMPONENTS_TRUSTED_VAULT_FRONTEND_TRUSTED_VAULT_CONNECTION_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}

namespace trusted_vault {

enum class SecurityDomainId;
class TrustedVaultConnection;

// Create a new `TrustedVaultConnection`. This is for cases where a
// security domain secret is managed outside of //components/trusted_vault.
// For most cases, see `TrustedVaultClient`.
std::unique_ptr<TrustedVaultConnection> NewFrontendTrustedVaultConnection(
    SecurityDomainId security_domain,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_FRONTEND_TRUSTED_VAULT_CONNECTION_H_
