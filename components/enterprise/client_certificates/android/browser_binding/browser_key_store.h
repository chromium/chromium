// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_STORE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_STORE_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "device/fido/public_key_credential_params.h"

namespace client_certificates {

class BrowserKey;
class BrowserKeyStore;

// Get a platform specific instance of the BrowserKeyStore. This function
// has per-platform implementations.
// TODO(crbug.com/432304139) Investigate if this can return a unique_ptr.
scoped_refptr<BrowserKeyStore> CreateBrowserKeyStoreInstance();

// An interface for creating storing and retrieving browser keys.
class BrowserKeyStore : public base::RefCounted<BrowserKeyStore> {
 public:
  using CredentialInfoList =
      std::vector<device::PublicKeyCredentialParams::CredentialInfo>;

  BrowserKeyStore() = default;
  BrowserKeyStore(const BrowserKeyStore&) = delete;
  BrowserKeyStore& operator=(const BrowserKeyStore&) = delete;

  // Get (or create if not present) a browser key for the given
  // credential_id.
  virtual std::unique_ptr<BrowserKey> GetOrCreateBrowserKeyForCredentialId(
      const std::vector<uint8_t>& credential_id,
      const CredentialInfoList& allowed_credentials) = 0;

  // Returns whether hardware keys are supported by this implementation on the
  // current device.
  virtual bool GetDeviceSupportsHardwareKeys() = 0;

  // Deletes the browser key for the given credential_id.
  virtual void DeleteBrowserKey(const std::vector<uint8_t>& credential_id) = 0;

 protected:
  virtual ~BrowserKeyStore() = default;

 private:
  friend base::RefCounted<BrowserKeyStore>;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_STORE_H_
