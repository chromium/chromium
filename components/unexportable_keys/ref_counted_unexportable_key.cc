// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/ref_counted_unexportable_key.h"

#include <memory>

#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

RefCountedUnexportableSigningKey::RefCountedUnexportableSigningKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key)
    : key_(std::move(key)) {
  CHECK(key_);
}

RefCountedUnexportableSigningKey::~RefCountedUnexportableSigningKey() = default;

crypto::UnexportableSigningKey& RefCountedUnexportableSigningKey::key() const {
  return *key_;
}

const UnexportableSigningKeyId& RefCountedUnexportableSigningKey::id() const {
  return id_;
}

RefCountedUnexportableAttestationKey::RefCountedUnexportableAttestationKey(
    std::unique_ptr<crypto::UnexportableAttestationKey> key)
    : key_(std::move(key)) {
  CHECK(key_);
}

RefCountedUnexportableAttestationKey::~RefCountedUnexportableAttestationKey() =
    default;

crypto::UnexportableAttestationKey& RefCountedUnexportableAttestationKey::key()
    const {
  return *key_;
}

const UnexportableAttestationKeyId& RefCountedUnexportableAttestationKey::id()
    const {
  return id_;
}

}  // namespace unexportable_keys
