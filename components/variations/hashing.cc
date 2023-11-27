// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/hashing.h"

#include <string.h>

#include "base/metrics/metrics_hashes.h"
#include "base/strings/stringprintf.h"

namespace variations {

uint32_t HashName(base::StringPiece name) {
  return base::HashFieldTrialName(name);
}

std::string HashNameAsHexString(base::StringPiece name) {
  return base::StringPrintf("%x", HashName(name));
}

}  // namespace variations
