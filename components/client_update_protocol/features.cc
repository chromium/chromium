// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_update_protocol/features.h"

namespace client_update_protocol::features {

// Controls whether Client Update Protocol (CUP) signing uses the Post-Quantum
// Cryptography (PQC) ML-DSA44 key instead of the pre-existing ECDSA key.
BASE_FEATURE(kPqcCupSigning,
             "PqcCupSigning",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace client_update_protocol::features
