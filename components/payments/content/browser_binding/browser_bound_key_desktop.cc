// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_desktop.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "crypto/cose.h"
#include "crypto/keypair.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

namespace {

constexpr char kSignLatencyHistogramName[] =
    "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKey.SignLatency";

}  // namespace

namespace payments {

BrowserBoundKeyDesktop::BrowserBoundKeyDesktop(
    std::unique_ptr<crypto::UnexportableSigningKey> key)
    : key_(std::move(key)) {
  CHECK(key_->Algorithm() ==
            crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256 ||
        key_->Algorithm() ==
            crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256);
}

BrowserBoundKeyDesktop::~BrowserBoundKeyDesktop() = default;

std::vector<uint8_t> BrowserBoundKeyDesktop::GetIdentifier() const {
  return key_->GetWrappedKey();
}

std::vector<uint8_t> BrowserBoundKeyDesktop::Sign(
    const std::vector<uint8_t>& client_data) {
  base::ElapsedTimer sign_timer;
  auto data = key_->SignSlowly(client_data).value_or(std::vector<uint8_t>());
  base::UmaHistogramTimes(kSignLatencyHistogramName, sign_timer.Elapsed());
  return data;
}

std::vector<uint8_t> BrowserBoundKeyDesktop::GetPublicKeyAsCoseKey() const {
  std::optional<crypto::keypair::PublicKey> public_key =
      crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
          key_->GetSubjectPublicKeyInfo());
  CHECK(public_key);

  return crypto::PublicKeyToCoseKey(public_key.value());
}

crypto::UnexportableSigningKey* BrowserBoundKeyDesktop::GetKeyForTesting() {
  return key_.get();
}

}  // namespace payments
