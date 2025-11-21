// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_store_desktop.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/browser_bound_key_desktop.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "device/fido/public_key_credential_params.h"

namespace {

constexpr char kGetKeyLatencyHistogramName[] =
    "PaymentRequest.SecurePaymentConfirmation."
    "BrowserBoundKeyStore.GetKeyLatency";
constexpr char kCreateKeyLatencyHistogramName[] =
    "PaymentRequest.SecurePaymentConfirmation."
    "BrowserBoundKeyStore.CreateKeyLatency";

#if BUILDFLAG(IS_MAC)
constexpr char kApplicationTag[] = "secure-payment-confirmation";
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

namespace payments {

scoped_refptr<BrowserBoundKeyStore> GetBrowserBoundKeyStoreInstance(
    BrowserBoundKeyStore::Config config) {
  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      crypto::GetUnexportableKeyProvider(
          crypto::UnexportableKeyProvider::Config{
#if BUILDFLAG(IS_MAC)
              .keychain_access_group = config.keychain_access_group,
              .application_tag = kApplicationTag,
              .access_control =
                  crypto::UnexportableKeyProvider::Config::AccessControl::kNone,
#endif  // BUILDFLAG(IS_MAC)
          });
  return base::MakeRefCounted<BrowserBoundKeyStoreDesktop>(
      std::move(key_provider));
}

BrowserBoundKeyStoreDesktop::BrowserBoundKeyStoreDesktop(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider)
    : key_provider_(std::move(key_provider)) {}

std::unique_ptr<BrowserBoundKey>
BrowserBoundKeyStoreDesktop::GetOrCreateBrowserBoundKeyForCredentialId(
    const std::vector<uint8_t>& credential_id,
    const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
        allowed_credentials) {
  if (!key_provider_) {
    return nullptr;
  }

  base::ElapsedTimer get_key_timer;
  std::unique_ptr<crypto::UnexportableSigningKey> key =
      key_provider_->FromWrappedSigningKeySlowly(credential_id);
  base::TimeDelta get_key_latency = get_key_timer.Elapsed();
  if (key) {
    base::UmaHistogramTimes(
        base::StrCat({kGetKeyLatencyHistogramName, ".KeyFound"}),
        get_key_latency);
    return std::make_unique<BrowserBoundKeyDesktop>(std::move(key));
  }
  // Note that if the key is found, there is an early return so we will only log
  // the latency once.
  base::UmaHistogramTimes(
      base::StrCat({kGetKeyLatencyHistogramName, ".KeyNotFound"}),
      get_key_latency);

  // No existing key, create a new one.
  base::ElapsedTimer create_key_timer;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> algorithms;
  algorithms.reserve(allowed_credentials.size());
  for (const auto& credential : allowed_credentials) {
    switch (
        static_cast<device::CoseAlgorithmIdentifier>(credential.algorithm)) {
      case device::CoseAlgorithmIdentifier::kRs256:
        algorithms.push_back(
            crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256);
        break;
      case device::CoseAlgorithmIdentifier::kEs256:
        algorithms.push_back(
            crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256);
        break;
      default:  // Unsupported algorithms.
        break;
    }
  }
  key = key_provider_->GenerateSigningKeySlowly(algorithms);
  base::UmaHistogramTimes(kCreateKeyLatencyHistogramName,
                          create_key_timer.Elapsed());
  return std::make_unique<BrowserBoundKeyDesktop>(std::move(key));
}

void BrowserBoundKeyStoreDesktop::DeleteBrowserBoundKey(
    std::vector<uint8_t> bbk_id) {
  if (crypto::StatefulUnexportableKeyProvider* stateful_provider =
          key_provider_ ? key_provider_->AsStatefulUnexportableKeyProvider()
                        : nullptr) {
    stateful_provider->DeleteSigningKeySlowly(bbk_id);
  }
}

bool BrowserBoundKeyStoreDesktop::GetDeviceSupportsHardwareKeys() {
#if BUILDFLAG(IS_MAC)
  return key_provider_ != nullptr;
#elif BUILDFLAG(IS_WIN)
  if (!key_provider_) {
    return false;
  }
  // On Windows, the existence of a key provider does not guarantee that
  // hardware-backed keys are supported. Check if we can create a key with
  // either of the two algorithms we support.
  return key_provider_->SelectAlgorithm(
             {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
              crypto::SignatureVerifier::SignatureAlgorithm::
                  RSA_PKCS1_SHA256}) != std::nullopt;
#else  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  // Hardware based browser bound keys are not supported on Linux or ChromeOS.
  return false;
#endif
}

BrowserBoundKeyStoreDesktop::~BrowserBoundKeyStoreDesktop() = default;

}  // namespace payments
