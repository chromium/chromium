// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"

#include "base/memory/weak_ptr.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "device/fido/public_key_credential_params.h"

namespace payments {

FakeBrowserBoundKeyStore::FakeBrowserBoundKeyStore() = default;

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

void FakeBrowserBoundKeyStore::DeleteBrowserBoundKey(
    std::vector<uint8_t> bbk_id) {
  key_map_.erase(bbk_id);
}

void FakeBrowserBoundKeyStore::PutFakeKey(
    FakeBrowserBoundKey bbk) {
  key_map_.insert(std::make_pair(bbk.GetIdentifier(), std::move(bbk)));
}

bool FakeBrowserBoundKeyStore::ContainsFakeKey(
    std::vector<uint8_t> bbk_id) const {
  return key_map_.contains(bbk_id);
}

FakeBrowserBoundKeyStore::~FakeBrowserBoundKeyStore() = default;

}  // namespace payments
