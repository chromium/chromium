// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/unexportable_key_utils.h"

#include <memory>

#include "base/feature_list.h"
#include "crypto/unexportable_key.h"
#include "device/fido/features.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webauthn/enclave_manager.h"
#endif  // BUILDFLAG(IS_MAC)

std::unique_ptr<crypto::UnexportableKeyProvider>
GetWebAuthnUnexportableKeyProvider() {
  if (base::FeatureList::IsEnabled(
          device::kWebAuthnUseInsecureSoftwareUnexportableKeys)) {
    return crypto::GetSoftwareUnsecureUnexportableKeyProvider();
  }
  crypto::UnexportableKeyProvider::Config config;
#if BUILDFLAG(IS_MAC)
  config.keychain_access_group =
      EnclaveManager::kEnclaveKeysKeychainAccessGroup;
#endif  // BUILDFLAG(IS_MAC)
  return crypto::GetUnexportableKeyProvider(std::move(config));
}
