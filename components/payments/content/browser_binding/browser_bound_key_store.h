// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_H_

#include <memory>
#include <vector>

namespace payments {

class BrowserBoundKey;
class BrowserBoundKeyStore;

// Get a platform specific instance of the BrowserBoundKeyStore. This function
// has per-platform implementations.
std::unique_ptr<BrowserBoundKeyStore> GetBrowserBoundKeyStoreInstance();

// An interface for creating storing and retrieving browser bound keys.
class BrowserBoundKeyStore {
 public:
  BrowserBoundKeyStore() = default;
  BrowserBoundKeyStore(const BrowserBoundKeyStore&) = delete;
  BrowserBoundKeyStore& operator=(const BrowserBoundKeyStore&) = delete;
  virtual ~BrowserBoundKeyStore() = default;

  // Get (or create if not present) a browser bound key for the given
  // credential_id.
  virtual std::unique_ptr<BrowserBoundKey>
  GetOrCreateBrowserBoundKeyForCredentialId(
      const std::vector<uint8_t>& credential_id) = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_H_
