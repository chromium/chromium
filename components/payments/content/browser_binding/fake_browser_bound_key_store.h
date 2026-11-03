// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_STORE_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_STORE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key.h"

namespace payments {

class BrowserBoundKey;

// An in-memory BrowserBoundKeyStore fake that returns keys pre-populated via
// `PutFakeKey()`. `GetOrCreateBrowserBoundKeyForCredentialId()` treats a stored
// key as newly created (requiring `allowed_algorithms` to match) when
// `FakeBrowserBoundKey::is_new()` is true, or as pre-existing (ignoring
// `allowed_algorithms`) when false.
//
// Example usage:
//   scoped_refptr<FakeBrowserBoundKeyStore> fake_key_store =
//       base::MakeRefCounted<FakeBrowserBoundKeyStore>();
//   fake_key_store->PutFakeKey(FakeBrowserBoundKey(
//       /*identifier=*/..., /*public_key_as_cose_key=*/...,
//       /*signature=*/..., /*algorithm_identifier=*/...,
//       /*expected_client_data=*/...));
class FakeBrowserBoundKeyStore : public BrowserBoundKeyStore {
 public:
  FakeBrowserBoundKeyStore();

  // BrowserBoundKeyStore:
  std::unique_ptr<BrowserBoundKey> GetOrCreateBrowserBoundKeyForCredentialId(
      const std::vector<uint8_t>& credential_id,
      const CredentialInfoList& allowed_algorithms) override;
  void DeleteBrowserBoundKey(std::vector<uint8_t> bbk_id) override;
  bool GetDeviceSupportsHardwareKeys() override;

  // Inserts (or replaces) a fake key. By default, the key is indexed by
  // `bbk.GetIdentifier()`. `requested_bbk_id` can be provided to simulate
  // platforms (such as Desktop) where key creation ignores the ID passed to
  // `GetOrCreateBrowserBoundKeyForCredentialId()` and generates a new
  // `bbk.GetIdentifier()`.
  void PutFakeKey(
      FakeBrowserBoundKey bbk,
      std::optional<std::vector<uint8_t>> requested_bbk_id = std::nullopt);

  // Returns whether the key with identifier `bbk_id` is present.
  bool ContainsFakeKey(const std::vector<uint8_t>& bbk_id) const;

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
