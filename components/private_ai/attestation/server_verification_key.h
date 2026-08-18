// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_ATTESTATION_SERVER_VERIFICATION_KEY_H_
#define COMPONENTS_PRIVATE_AI_ATTESTATION_SERVER_VERIFICATION_KEY_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "url/gurl.h"

namespace private_ai {

enum class OutputPrefixType : int {
  TINK = 1,
  LEGACY = 2,
};

struct ProcessedKey {
  uint32_t id;
  OutputPrefixType output_prefix_type;
  const char* x;  // 32 bytes
  const char* y;  // 32 bytes

  bool operator==(const ProcessedKey& other) const;
};

// Returns the server verification key based on the URL. The steps to update
// this key are in b/469921004.
base::span<const ProcessedKey> GetServerVerificationKey(const GURL& url);

// Returns whether the URL targets a non-prod server verification key variant.
bool IsNonProdServerVerificationKey(const GURL& url);

// The following functions are for testing only and return the server
// verification keys for the corresponding environment.
base::span<const ProcessedKey> GetAutopushKeysForTesting();
base::span<const ProcessedKey> GetDevKeysForTesting();
base::span<const ProcessedKey> GetLabsKeysForTesting();
base::span<const ProcessedKey> GetProdKeysForTesting();

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_ATTESTATION_SERVER_VERIFICATION_KEY_H_
