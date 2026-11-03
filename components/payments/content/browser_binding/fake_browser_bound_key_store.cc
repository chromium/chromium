// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"

#include <algorithm>
#include <utility>

#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key.h"
#include "device/fido/public/public_key_credential_params.h"

namespace payments {

FakeBrowserBoundKeyStore::FakeBrowserBoundKeyStore() = default;

std::unique_ptr<BrowserBoundKey>
FakeBrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId(
    const std::vector<uint8_t>& credential_id,
    const CredentialInfoList& allowed_algorithms) {
  auto it = key_map_.find(credential_id);
  if (it == key_map_.end()) {
    return nullptr;
  }
  const FakeBrowserBoundKey& bbk = it->second;
  if (bbk.is_new() &&
      !std::ranges::contains(
          allowed_algorithms, bbk.algorithm_identifier(),
          &device::PublicKeyCredentialParams::CredentialInfo::algorithm)) {
    return nullptr;
  }
  return std::make_unique<FakeBrowserBoundKey>(bbk);
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
    std::optional<std::vector<uint8_t>> requested_bbk_id) {
  std::vector<uint8_t> key_id = requested_bbk_id.value_or(bbk.GetIdentifier());
  key_map_.insert_or_assign(std::move(key_id), std::move(bbk));
}

bool FakeBrowserBoundKeyStore::ContainsFakeKey(
    const std::vector<uint8_t>& bbk_id) const {
  return key_map_.contains(bbk_id);
}

FakeBrowserBoundKeyStore::~FakeBrowserBoundKeyStore() = default;

}  // namespace payments
