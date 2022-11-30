// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_key_util.h"

#include <utility>

namespace ownership {

///////////////////////////////////////////////////////////////////////////
// PublicKey

PublicKey::PublicKey(bool is_persisted, std::vector<uint8_t> data)
    : is_persisted_(is_persisted), data_(std::move(data)) {}

PublicKey::~PublicKey() = default;

scoped_refptr<PublicKey> PublicKey::clone() {
  return base::MakeRefCounted<ownership::PublicKey>(is_persisted_, data_);
}

///////////////////////////////////////////////////////////////////////////
// PrivateKey

PrivateKey::PrivateKey(crypto::ScopedSECKEYPrivateKey key)
    : key_(std::move(key)) {}

PrivateKey::~PrivateKey() = default;

}  // namespace ownership
