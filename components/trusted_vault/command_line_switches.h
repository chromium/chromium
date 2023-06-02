// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_COMMAND_LINE_SWITCHES_H_
#define COMPONENTS_TRUSTED_VAULT_COMMAND_LINE_SWITCHES_H_

class GURL;

namespace trusted_vault {

// Specifies the vault server used for trusted vault passphrase.
inline constexpr char kTrustedVaultServiceURLSwitch[] =
    "trusted-vault-service-url";

// Returns the default URL for the trusted vault server or the override
// specified via `kTrustedVaultServiceURLSwitch`.
GURL ExtractTrustedVaultServiceURLFromCommandLine();

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_COMMAND_LINE_SWITCHES_H_
