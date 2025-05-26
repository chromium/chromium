// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "device/fido/public_key_credential_params.h"

namespace payments {

class BrowserBoundKey;
class BrowserBoundKeyStore;

// Get a platform specific instance of the BrowserBoundKeyStore. This function
// has per-platform implementations.
scoped_refptr<BrowserBoundKeyStore> GetBrowserBoundKeyStoreInstance();

// An interface for creating storing and retrieving browser bound keys.
class BrowserBoundKeyStore : public base::RefCounted<BrowserBoundKeyStore> {
 public:
  using CredentialInfoList =
      std::vector<device::PublicKeyCredentialParams::CredentialInfo>;
  BrowserBoundKeyStore() = default;
  BrowserBoundKeyStore(const BrowserBoundKeyStore&) = delete;
  BrowserBoundKeyStore& operator=(const BrowserBoundKeyStore&) = delete;

  // Get (or create if not present) a browser bound key for the given
  // credential_id.
  // `allowed_credentials` is a vector of COSE Algorithm identifiers that
  // restricts the algorithms of the browser bound key when creating a new key.
  virtual std::unique_ptr<BrowserBoundKey>
  GetOrCreateBrowserBoundKeyForCredentialId(
      const std::vector<uint8_t>& credential_id,
      const CredentialInfoList& allowed_credentials) = 0;

  // Deletes the browser bound key, given its identifier.
  // `bbk_id` is the identifier of the BrowserBoundKey. Use `std::move()` when
  // appropriate to avoid copying the `bbk_id` vector.
  virtual void DeleteBrowserBoundKey(std::vector<uint8_t> bbk_id) = 0;

 protected:
  virtual ~BrowserBoundKeyStore() = default;

 private:
  friend base::RefCounted<BrowserBoundKeyStore>;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_H_
