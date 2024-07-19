// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_HYBRID_ENCRYPTION_KEY_TEST_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_HYBRID_ENCRYPTION_KEY_TEST_UTILS_H_

#include <cstdint>
#include <vector>

class HybridEncryptionKey;

// Returns Hybrid key's public key that's not publicly exposed by the
// `HybridEncryptionKey` class.
std::vector<uint8_t> GetHybridEncryptionPublicKeyForTesting(
    const HybridEncryptionKey& key);

// Returns a valid, fixed `HybridEncryptionKey` for testing. Prefer using this
// function instead of generating random keys in tests for greater determinism.
HybridEncryptionKey CreateHybridEncryptionKeyForTesting();

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_HYBRID_ENCRYPTION_KEY_TEST_UTILS_H_
