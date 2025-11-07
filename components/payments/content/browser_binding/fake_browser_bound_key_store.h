// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_STORE_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_STORE_H_

#include <cstdint>
#include <map>
#include <vector>

#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key.h"

namespace payments {

class BrowserBoundKey;

// A fake used in tests to provide BrowserBoundKeyStore.
//
// scoped_refptr<FakeBrowserBoundKeyStore> fake_key_store =
//     base::MakeRefCounted<FakeBrowserBoundKeyStore>();
// fake_key_store.PutFakeKey(
//     FakeBrowserBoundKey(/*browser_bound_key_id=*/..., /*public_key=*/...,
//         /*signature=*/..., expected_client_data=*/...);
class FakeBrowserBoundKeyStore : public BrowserBoundKeyStore {
 public:
  FakeBrowserBoundKeyStore();

  // BrowserBoundKeyStore:
  std::unique_ptr<BrowserBoundKey> GetOrCreateBrowserBoundKeyForCredentialId(
      const std::vector<uint8_t>& credential_id,
      const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
          allowed_algorithms) override;
  void DeleteBrowserBoundKey(std::vector<uint8_t> bbk_id) override;
  bool GetDeviceSupportsHardwareKeys() override;

  // Insert a fake key. If the optional `bbk_id` is not provided, the key's own
  // identifier will be used as the map key for retrieving the BBK.
  void PutFakeKey(FakeBrowserBoundKey bbk,
                  std::optional<std::vector<uint8_t>> bbk_id = std::nullopt);

  // Return whether the key with identifier `bbk_id` is present.
  bool ContainsFakeKey(std::vector<uint8_t> bbk_id) const;

  // Sets the value returned by GetDeviceSupportsHardwareKeys().
  void SetDeviceSupportsHardwareKeys(bool value) {
    device_supports_hardware_keys_ = value;
  }

 protected:
  ~FakeBrowserBoundKeyStore() override;

 private:
  std::map<std::vector<uint8_t>, FakeBrowserBoundKey> key_map_;
  bool device_supports_hardware_keys_ = true;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_STORE_H_
