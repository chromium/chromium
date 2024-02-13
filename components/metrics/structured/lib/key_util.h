// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_KEY_UTIL_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_KEY_UTIL_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "components/metrics/structured/lib/proto/key.pb.h"

namespace metrics::structured {

// Key size to hash strings for structured metrics.
inline constexpr size_t kKeySize = 32;

namespace util {

// Generates a new key to be used for hashing. This function should be used to
// create new keys or to replace a key that needs to be rotated.
std::string GenerateNewKey();

// Helper conversion function between Value and KeyProto.
base::Value CreateValueFromKeyProto(const KeyProto& proto);
std::optional<KeyProto> CreateKeyProtoFromValue(const base::Value::Dict& value);

}  // namespace util
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_KEY_UTIL_H_
