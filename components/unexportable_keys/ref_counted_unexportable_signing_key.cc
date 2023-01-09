// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"

#include <memory>

#include "crypto/unexportable_key.h"

namespace unexportable_keys {

namespace {
RefCountedUnexportableSigningKey::KeyId GetNextKeyId() {
  static uint32_t next_id = 0;
  return RefCountedUnexportableSigningKey::KeyId(next_id++);
}
}  // namespace

RefCountedUnexportableSigningKey::RefCountedUnexportableSigningKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key)
    : key_(std::move(key)), key_id_(GetNextKeyId()) {
  DCHECK(key_);
}

RefCountedUnexportableSigningKey::~RefCountedUnexportableSigningKey() = default;

}  // namespace unexportable_keys
