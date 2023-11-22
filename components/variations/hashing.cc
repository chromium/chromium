// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/hashing.h"

#include "base/metrics/metrics_hashes.h"

namespace variations {

uint32_t HashName(base::StringPiece name) {
  return base::HashFieldTrialName(name);
}

}  // namespace variations
