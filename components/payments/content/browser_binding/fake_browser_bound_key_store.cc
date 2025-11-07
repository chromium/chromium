// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"

#include "base/memory/weak_ptr.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key.h"
#include "device/fido/public_key_credential_params.h"

namespace payments {

FakeBrowserBoundKeyStore::FakeBrowserBoundKeyStore() = default;

std::unique_ptr<BrowserBoundKey>
FakeBrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId(
    const std::vector<uint8_t>& credential_id,
    const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
        allowed_algorithms) {
  auto it = key_map_.find(credential_id);
  if (it == key_map_.end()) {
    return nullptr;
  }
  if (!it->second.is_new()) {
    return std::make_unique<FakeBrowserBoundKey>(it->second);
  }
  for (auto& allowed_algorithm : allowed_algorithms) {
    if (it->second.algorithm_identifier() == allowed_algorithm.algorithm) {
      return std::make_unique<FakeBrowserBoundKey>(it->second);
    }
  }
  return nullptr;
}

void FakeBrowserBoundKeyStore::DeleteBrowserBoundKey(
    std::vector<uint8_t> bbk_id) {
  key_map_.erase(bbk_id);
}

bool FakeBrowserBoundKeyStore::GetDeviceSupportsHardwareKeys() {
  return device_supports_hardware_keys_;
}

void FakeBrowserBoundKeyStore::PutFakeKey(
    FakeBrowserBoundKey bbk,
    std::optional<std::vector<uint8_t>> bbk_id) {
  key_map_.insert(
      std::make_pair(bbk_id.has_value() ? bbk_id.value() : bbk.GetIdentifier(),
                     std::move(bbk)));
}

bool FakeBrowserBoundKeyStore::ContainsFakeKey(
    std::vector<uint8_t> bbk_id) const {
  return key_map_.contains(bbk_id);
}

FakeBrowserBoundKeyStore::~FakeBrowserBoundKeyStore() = default;

}  // namespace payments
