// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_service_util.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/rand_util.h"

namespace invalidation {

std::string GenerateInvalidatorClientId() {
  // Generate a GUID with 128 bits worth of base64-encoded randomness.
  // This format is similar to that of sync's cache_guid.
  const int kGuidBytes = 128 / 8;
  std::string guid = base::Base64Encode(base::RandBytesAsVector(kGuidBytes));
  DCHECK(!guid.empty());
  return guid;
}

}  // namespace invalidation
