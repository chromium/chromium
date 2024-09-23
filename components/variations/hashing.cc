// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/hashing.h"

#include <string.h>

#include <string_view>

#include "base/metrics/metrics_hashes.h"
#include "base/strings/stringprintf.h"

namespace variations {

uint32_t HashName(std::string_view name) {
  return base::HashFieldTrialName(name);
}

std::string HashNameAsHexString(std::string_view name) {
  return base::StringPrintf("%x", HashName(name));
}

}  // namespace variations
