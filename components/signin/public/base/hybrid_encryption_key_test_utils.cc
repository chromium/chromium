// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/hybrid_encryption_key_test_utils.h"

#include "components/signin/public/base/hybrid_encryption_key.h"

std::vector<uint8_t> GetHybridEncryptionPublicKeyForTesting(
    const HybridEncryptionKey& key) {
  return key.GetPublicKey();
}

HybridEncryptionKey CreateHybridEncryptionKeyForTesting() {
  static const uint8_t kPrivateKey[X25519_PRIVATE_KEY_LEN] = {
      0xbc, 0xb5, 0x51, 0x29, 0x31, 0x10, 0x30, 0xc9, 0xed, 0x26, 0xde,
      0xd4, 0xb3, 0xdf, 0x3a, 0xce, 0x06, 0x8a, 0xee, 0x17, 0xab, 0xce,
      0xd7, 0xdb, 0xf3, 0x11, 0xe5, 0xa8, 0xf3, 0xb1, 0x8e, 0x24};
  return HybridEncryptionKey(kPrivateKey);
}
