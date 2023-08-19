// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"

namespace syncer {

CrossUserSharingPublicKey::CrossUserSharingPublicKey(
    CrossUserSharingPublicKey&& other) = default;
CrossUserSharingPublicKey& CrossUserSharingPublicKey::operator=(
    CrossUserSharingPublicKey&& other) = default;
CrossUserSharingPublicKey::~CrossUserSharingPublicKey() = default;

CrossUserSharingPublicKey::CrossUserSharingPublicKey(
    base::span<const uint8_t> public_key) {
  CHECK_EQ(static_cast<size_t>(X25519_PUBLIC_VALUE_LEN), public_key.size());

  std::copy(public_key.begin(), public_key.end(), public_key_);
}

// static
absl::optional<CrossUserSharingPublicKey>
CrossUserSharingPublicKey::CreateByImport(
    base::span<const uint8_t> public_key) {
  if (public_key.size() != X25519_PUBLIC_VALUE_LEN) {
    return {};
  }
  return CrossUserSharingPublicKey(public_key);
}

std::array<uint8_t, X25519_PUBLIC_VALUE_LEN>
CrossUserSharingPublicKey::GetRawPublicKey() const {
  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> raw_public_key;
  std::copy(public_key_, public_key_ + X25519_PUBLIC_VALUE_LEN,
            raw_public_key.begin());
  return raw_public_key;
}

CrossUserSharingPublicKey CrossUserSharingPublicKey::Clone() const {
  return CrossUserSharingPublicKey(GetRawPublicKey());
}

}  // namespace syncer
