// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_SERVICE_ERROR_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_SERVICE_ERROR_H_

#include "base/types/expected.h"

namespace unexportable_keys {

// Various errors returned by this component.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ServiceError {
  // Reserved for histograms.
  // kNone = 0
  // crypto:: operation returned an error.
  kCryptoApiFailed = 1,
  // Provided key ID is unknown and doesn't correspond to any key.
  kKeyNotFound = 2,
  // Newly generated key is the same as the existing one (should be extremely
  // rare).
  kKeyCollision = 3,
  // Unexportable key provider is not available on this platform.
  kNoKeyProvider = 4,
  // None of the requested algorithms are supported by the key provider.
  kAlgorithmNotSupported = 5,
  // The key object hasn't been created yet. Try again later.
  kKeyNotReady = 6,

  kMaxValue = kKeyNotReady
};

// Fake `ServiceError` value that can be used for metrics to signify that no
// error has occurred.
constexpr ServiceError kNoServiceErrorForMetrics = static_cast<ServiceError>(0);

// Return value for methods which perform unexportable keys operations that may
// fail. Either contains a `ServiceError` or a result value of arbitrary type.
template <class Result>
using ServiceErrorOr = base::expected<Result, ServiceError>;

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_SERVICE_ERROR_H_
