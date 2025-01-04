// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_STORE_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_STORE_H_

#include <cstdint>
#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key.h"

namespace payments {

class BrowserBoundKey;

// A fake used in tests to provide BrowserBoundKeyStore.
//
// // SetUp
// auto fake_key_store = std::make_unique<FakeBrowserBoundKeyStore>();
// fake_key_store_weak_ptr_ = fake_key_store.GetWeakPtr();
// instance_under_test.SetKeyStoreForTesting(
//     base::WrapUnique<BrowserBoundKeyStore*>(fake_key_store.release()));
// ...
// // Test
// fake_key_store_weak_ptr_.PutFakeKey(credential_id,
//     FakeBrowserBoundKey(/*public_key=*/...,
//         /*signature=*/..., expected_client_data=*/...);
class FakeBrowserBoundKeyStore : public BrowserBoundKeyStore {
 public:
  FakeBrowserBoundKeyStore();
  ~FakeBrowserBoundKeyStore() override;

  std::unique_ptr<BrowserBoundKey> GetOrCreateBrowserBoundKeyForCredentialId(
      const std::vector<uint8_t>& credential_id) override;

  void PutFakeKey(const std::vector<uint8_t>& credential_id,
                  FakeBrowserBoundKey bbk);

  base::WeakPtr<FakeBrowserBoundKeyStore> GetWeakPtr();

 private:
  std::map<std::vector<uint8_t>, FakeBrowserBoundKey> key_map_;
  base::WeakPtrFactory<FakeBrowserBoundKeyStore> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_STORE_H_
