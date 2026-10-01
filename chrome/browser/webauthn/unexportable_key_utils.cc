// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/unexportable_key_utils.h"

#include <atomic>
#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "crypto/unexportable_key.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/public/features.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webauthn/enclave_manager.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

std::unique_ptr<crypto::UnexportableKeyProvider> (*g_mock_provider)() = nullptr;

// Whether `kUnexportableKeyProviderTypeHistogram` has already been recorded in
// this process.
std::atomic<bool> g_provider_type_recorded = false;

void RecordUnexportableKeyProviderType(UnexportableKeyProviderType type) {
  // The result is not expected to change during the lifetime of the process,
  // so only record it once.
  if (g_provider_type_recorded.exchange(true)) {
    return;
  }
  base::UmaHistogramEnumeration(kUnexportableKeyProviderTypeHistogram, type);
}

bool SupportsEnclaveSigningAlgorithms(
    crypto::UnexportableKeyProvider* provider) {
  return provider && provider->SelectAlgorithm(
                         device::enclave::kSigningAlgorithms) != std::nullopt;
}

}  // namespace

std::unique_ptr<crypto::UnexportableKeyProvider>
GetWebAuthnUnexportableKeyProvider() {
  // The WebAuthn test override takes preference.
  if (g_mock_provider) {
    return g_mock_provider();
  }

  if (base::FeatureList::IsEnabled(
          device::kWebAuthnUseInsecureSoftwareUnexportableKeys)) {
    return crypto::GetSoftwareUnsecureUnexportableKeyProvider();
  }

  crypto::UnexportableKeyProvider::Config config;
#if BUILDFLAG(IS_MAC)
  config.keychain_access_group =
      EnclaveManager::kEnclaveKeysKeychainAccessGroup;
#endif  // BUILDFLAG(IS_MAC)
  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      crypto::GetUnexportableKeyProvider(std::move(config));
  if (SupportsEnclaveSigningAlgorithms(provider.get())) {
    RecordUnexportableKeyProviderType(UnexportableKeyProviderType::kHardware);
    return provider;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows, if there is no TPM support, use the Microsoft Software Key
  // Storage Provider instead.
  provider = crypto::GetMicrosoftSoftwareUnexportableKeyProvider();
  if (SupportsEnclaveSigningAlgorithms(provider.get())) {
    RecordUnexportableKeyProviderType(
        UnexportableKeyProviderType::kMicrosoftSoftware);
    return provider;
  }
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // On Linux, access to the TPM is complex compared to Windows and macOS.
  // There are libraries that _should_ work with a TPM 2.0, but Linux
  // often runs on non-PCs, where TPMs will probably never exist. Thus
  // gating enclave features on the presence of a TPM isn't viable, and
  // trying to use one where it is present seems complex and likely to
  // cause lots of problems.
  //
  // ChromeOS would need to implement support for unexportable keys backed
  // by TPM/H1 in a system daemon. This doesn't exist at present.
  //
  // For these platforms without hardware-backed key support, save
  // identity keys on disk using the software unexportable key provider.
  RecordUnexportableKeyProviderType(
      UnexportableKeyProviderType::kSoftwareFallback);
  return crypto::GetSoftwareUnsecureUnexportableKeyProvider();
#else
  // Some macOS devices lack a Secure Enclave (TPM), and some older
  // Windows devices may lack both a TPM and Microsoft Software Key
  // Storage Provider support.
  //
  // For these devices, save identity keys on disk using the software
  // unexportable key provider if the feature is enabled.
  if (base::FeatureList::IsEnabled(
          device::kWebAuthnSoftwareKeysWhenTpmAbsent)) {
    RecordUnexportableKeyProviderType(
        UnexportableKeyProviderType::kSoftwareFallback);
    return crypto::GetSoftwareUnsecureUnexportableKeyProvider();
  }
  RecordUnexportableKeyProviderType(UnexportableKeyProviderType::kNone);
  return nullptr;
#endif
}

std::unique_ptr<crypto::UserVerifyingKeyProvider>
GetWebAuthnUserVerifyingKeyProvider(
    crypto::UserVerifyingKeyProvider::Config config) {
  return crypto::GetUserVerifyingKeyProvider(std::move(config));
}

void SetWebAuthnUnexportableKeyProviderForTesting(
    std::unique_ptr<crypto::UnexportableKeyProvider> (*func)()) {
  if (g_mock_provider) {
    // Nesting scoped providers is not supported.
    CHECK(!func);
    g_mock_provider = nullptr;
  } else {
    g_mock_provider = func;
  }
}

void ResetUnexportableKeyProviderTypeMetricForTesting() {
  g_provider_type_recorded = false;
}
