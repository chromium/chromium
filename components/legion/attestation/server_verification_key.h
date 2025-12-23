// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CRYPTO_SERVER_VERIFICATION_KEY_H_
#define COMPONENTS_LEGION_CRYPTO_SERVER_VERIFICATION_KEY_H_

#include <stdint.h>

#include "base/containers/span.h"

namespace legion {

// Returns the server verification key. The steps to update this key are in
// b/469921004.
base::span<const uint8_t> GetDevServerVerificationKey();

}  // namespace legion

#endif  // COMPONENTS_LEGION_CRYPTO_SERVER_VERIFICATION_KEY_H_
