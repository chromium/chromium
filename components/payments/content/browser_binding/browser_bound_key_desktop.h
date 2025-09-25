// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DESKTOP_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DESKTOP_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "components/payments/content/browser_binding/browser_bound_key.h"

namespace crypto {
class UnexportableSigningKey;
}  // namespace crypto

namespace payments {

// Implements BrowserBoundKey for Desktop.
class BrowserBoundKeyDesktop : public BrowserBoundKey {
 public:
  // The UnexportableSigningKey passed in must use either the ECDSA_SHA256 or
  // the RSA_PKCS1_SHA256 algorithm.
  explicit BrowserBoundKeyDesktop(
      std::unique_ptr<crypto::UnexportableSigningKey> key);
  ~BrowserBoundKeyDesktop() override;

  // BrowserBoundKey:
  std::vector<uint8_t> GetIdentifier() const override;
  std::vector<uint8_t> Sign(const std::vector<uint8_t>& client_data) override;
  std::vector<uint8_t> GetPublicKeyAsCoseKey() const override;

  crypto::UnexportableSigningKey* GetKeyForTesting();

 private:
  std::unique_ptr<crypto::UnexportableSigningKey> key_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DESKTOP_H_
