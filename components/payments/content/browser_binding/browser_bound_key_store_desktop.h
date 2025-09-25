// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_DESKTOP_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_DESKTOP_H_

#include <stdint.h>

#include "components/payments/content/browser_binding/browser_bound_key_store.h"

namespace crypto {
class UnexportableKeyProvider;
}

namespace payments {

class BrowserBoundKey;

// Implements BrowserBoundKeyStore for Desktop.
class BrowserBoundKeyStoreDesktop : public BrowserBoundKeyStore {
 public:
  explicit BrowserBoundKeyStoreDesktop(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider);

  // BrowserBoundKeyStore:
  std::unique_ptr<BrowserBoundKey> GetOrCreateBrowserBoundKeyForCredentialId(
      const std::vector<uint8_t>& credential_id,
      const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
          allowed_credentials) override;
  void DeleteBrowserBoundKey(std::vector<uint8_t> bbk_id) override;
  bool GetDeviceSupportsHardwareKeys() override;

 protected:
  ~BrowserBoundKeyStoreDesktop() override;

 private:
  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_DESKTOP_H_
