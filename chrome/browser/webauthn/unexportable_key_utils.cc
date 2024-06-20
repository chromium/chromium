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
  constexpr bool kIsLinux =
#if BUILDFLAG(IS_LINUX)
      true;
#else
      false;
#endif

  // On Linux, access to the TPM is complex compared to Windows and macOS.
  // There are libraries that _should_ work with a TPM 2.0, but Linux often
  // runs on non-PCs, where TPMs will probably never exist. Thus gating enclave
  // features on the presence of a TPM isn't viable, and trying to use one
  // where it is present seems complex and likely to cause lots of problems.
  // Thus Linux saves identity keys on disk.
  //
  // If there is a scoped UnexportableKeyProvider configured, we always use
  // that so that tests can still override the key provider.
  if ((kIsLinux && !crypto::internal::HasScopedUnexportableKeyProvider()) ||
      base::FeatureList::IsEnabled(
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
