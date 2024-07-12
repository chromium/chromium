// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"

#include <array>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"

namespace syncer {

CrossUserSharingPublicKey::CrossUserSharingPublicKey(
    CrossUserSharingPublicKey&& other) = default;
CrossUserSharingPublicKey& CrossUserSharingPublicKey::operator=(
    CrossUserSharingPublicKey&& other) = default;
CrossUserSharingPublicKey::~CrossUserSharingPublicKey() = default;

CrossUserSharingPublicKey::CrossUserSharingPublicKey(
    base::span<const uint8_t, X25519_PUBLIC_VALUE_LEN> public_key) {
  base::span(public_key_).copy_from(public_key);
}

// static
std::optional<CrossUserSharingPublicKey>
CrossUserSharingPublicKey::CreateByImport(
    base::span<const uint8_t> public_key) {
  if (public_key.size() != X25519_PUBLIC_VALUE_LEN) {
    return {};
  }
  return CrossUserSharingPublicKey(public_key.first<X25519_PUBLIC_VALUE_LEN>());
}

std::array<uint8_t, X25519_PUBLIC_VALUE_LEN>
CrossUserSharingPublicKey::GetRawPublicKey() const {
  return public_key_;
}

CrossUserSharingPublicKey CrossUserSharingPublicKey::Clone() const {
  return CrossUserSharingPublicKey(GetRawPublicKey());
}

}  // namespace syncer
