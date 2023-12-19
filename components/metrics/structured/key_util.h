// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_UTIL_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_UTIL_H_

#include <string>

namespace metrics::structured {

// Key size to hash strings for structured metrics.
inline constexpr size_t kKeySize = 32;

namespace util {

// Generates a new key to be used for hashing. This function should be used to
// create new keys or to replace a key that needs to be rotated.
std::string GenerateNewKey();

}  // namespace util
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_KEY_UTIL_H_
