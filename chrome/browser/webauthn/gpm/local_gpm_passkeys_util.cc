// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/local_gpm_passkeys_util.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "components/trusted_vault/features.h"
#include "device/fido/public/features.h"

namespace webauthn {

bool IsLocalGpmPasskeysEnabled() {
  return base::FeatureList::IsEnabled(device::kWebAuthnLocalGpmPasskeys) &&
         base::FeatureList::IsEnabled(
             trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption);
}

void ShouldUseLocalGpmPasskeys(Profile* profile,
                               base::OnceCallback<void(bool)> callback) {
  CHECK(profile);
  if (!IsLocalGpmPasskeysEnabled()) {
    std::move(callback).Run(false);
    return;
  }

  switch (device::kWebAuthnLocalGpmPasskeysMode.Get()) {
    case device::LocalGpmPasskeysMode::kAllUsers:
      std::move(callback).Run(true);
      return;
    case device::LocalGpmPasskeysMode::kNewUsersOnly: {
      EnclaveManagerInterface* enclave_manager =
          EnclaveManagerFactory::GetForProfile(profile);
      if (!enclave_manager) {
        std::move(callback).Run(true);
        return;
      }
      enclave_manager->LoadAndCheckIsRegistered(
          base::BindOnce([](bool is_registered) {
            return !is_registered;
          }).Then(std::move(callback)));
      return;
    }
  }
}

}  // namespace webauthn
