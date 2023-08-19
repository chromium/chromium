// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair(
    CrossUserSharingPublicPrivateKeyPair&& other) = default;

CrossUserSharingPublicPrivateKeyPair&
CrossUserSharingPublicPrivateKeyPair::operator=(
    CrossUserSharingPublicPrivateKeyPair&& other) = default;

CrossUserSharingPublicPrivateKeyPair::~CrossUserSharingPublicPrivateKeyPair() =
    default;

// static
CrossUserSharingPublicPrivateKeyPair
CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair() {
  return CrossUserSharingPublicPrivateKeyPair();
}

// static
absl::optional<CrossUserSharingPublicPrivateKeyPair>
CrossUserSharingPublicPrivateKeyPair::CreateByImport(
    base::span<const uint8_t> private_key) {
  if (private_key.size() != X25519_PRIVATE_KEY_LEN) {
    return {};
  }
  return CrossUserSharingPublicPrivateKeyPair(std::move(private_key));
}

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair(
    base::span<const uint8_t> private_key) {
  CHECK_EQ(static_cast<size_t>(X25519_PRIVATE_KEY_LEN), private_key.size());

  std::copy(private_key.begin(), private_key.end(), private_key_);
  X25519_public_from_private(public_key_, private_key_);
}

CrossUserSharingPublicPrivateKeyPair::CrossUserSharingPublicPrivateKeyPair() {
  X25519_keypair(public_key_, private_key_);
}

std::array<uint8_t, X25519_PRIVATE_KEY_LEN>
CrossUserSharingPublicPrivateKeyPair::GetRawPrivateKey() const {
  std::array<uint8_t, X25519_PRIVATE_KEY_LEN> raw_private_key;
  std::copy(private_key_, private_key_ + X25519_PRIVATE_KEY_LEN,
            raw_private_key.begin());
  return raw_private_key;
}

std::array<uint8_t, X25519_PUBLIC_VALUE_LEN>
CrossUserSharingPublicPrivateKeyPair::GetRawPublicKey() const {
  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> raw_public_key;
  std::copy(public_key_, public_key_ + X25519_PUBLIC_VALUE_LEN,
            raw_public_key.begin());
  return raw_public_key;
}

}  // namespace syncer
