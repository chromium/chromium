// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_key_util.h"

#include <utility>

namespace ownership {

///////////////////////////////////////////////////////////////////////////
// PublicKey

PublicKey::PublicKey() {
}

PublicKey::~PublicKey() {
}

///////////////////////////////////////////////////////////////////////////
// PrivateKey

PrivateKey::PrivateKey(crypto::ScopedSECKEYPrivateKey key)
    : key_(std::move(key)) {}

PrivateKey::~PrivateKey() {
}

}  // namespace ownership
