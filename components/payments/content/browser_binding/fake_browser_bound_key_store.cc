// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"

#include "base/memory/weak_ptr.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "device/fido/public_key_credential_params.h"

namespace payments {

FakeBrowserBoundKeyStore::FakeBrowserBoundKeyStore() = default;
FakeBrowserBoundKeyStore::~FakeBrowserBoundKeyStore() = default;

std::unique_ptr<BrowserBoundKey>
FakeBrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId(
    const std::vector<uint8_t>& credential_id,
    const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
        allowed_credentials) {
  auto it = key_map_.find(credential_id);
  if (it == key_map_.end()) {
    return nullptr;
  }
  for (auto& credential_info : allowed_credentials) {
    if (it->second.algorithm_identifier() == credential_info.algorithm) {
      return std::unique_ptr<BrowserBoundKey>(
          new FakeBrowserBoundKey(it->second));
    }
  }
  return nullptr;
}

void FakeBrowserBoundKeyStore::PutFakeKey(
    const std::vector<uint8_t>& credential_id,
    FakeBrowserBoundKey bbk) {
  key_map_.insert(std::make_pair(credential_id, std::move(bbk)));
}

base::WeakPtr<FakeBrowserBoundKeyStore> FakeBrowserBoundKeyStore::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
