// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CRYPTO_CONSTANTS_H_
#define COMPONENTS_PRIVATE_AI_CRYPTO_CONSTANTS_H_

#include <stddef.h>

namespace private_ai {

// Length of a P-256 public key in uncompressed X9.62 format.
inline constexpr size_t kP256X962Length = 65;

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CRYPTO_CONSTANTS_H_
