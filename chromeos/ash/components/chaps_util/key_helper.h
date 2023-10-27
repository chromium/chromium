// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_KEY_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_KEY_HELPER_H_

#include <stdint.h>

#include <vector>

#include "crypto/scoped_nss_types.h"

namespace chromeos {

// Calculates and returns CKA_ID from public key bytes (`public_key_bytes`).
crypto::ScopedSECItem MakeIdFromPubKeyNss(
    const std::vector<uint8_t>& public_key_bytes);

// Converts ScopedSECItem id (`id`) to vector<uint8_t>.
std::vector<uint8_t> SECItemToBytes(const crypto::ScopedSECItem& id);

}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_KEY_HELPER_H_
