// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"

#include <memory>

#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

RefCountedUnexportableSigningKey::RefCountedUnexportableSigningKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key,
    const UnexportableKeyId& key_id)
    : key_(std::move(key)), id_(key_id) {
  DCHECK(key_);
}

RefCountedUnexportableSigningKey::~RefCountedUnexportableSigningKey() = default;

}  // namespace unexportable_keys
