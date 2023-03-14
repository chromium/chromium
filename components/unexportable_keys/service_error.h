// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_SERVICE_ERROR_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_SERVICE_ERROR_H_

#include "base/types/expected.h"

namespace unexportable_keys {

// Various errors returned by `UnexportableKeyService`.
enum class ServiceError {
  // crypto:: operation returned an error.
  kCryptoApiFailed,
  // Provided key ID is unknown and doesn't correspond to any key.
  kKeyNotFound,
  // Newly generated key is the same as the existing one (should be extremely
  // rare).
  kKeyCollision,
  // Unexportable key provider is not available on this platform.
  kNoKeyProvider,
  // None of the requested algorithms are supported by the key provider.
  kAlgorithmNotSupported
};

// Return value for methods which perform unexportable keys operations that may
// fail. Either contains a `ServiceError` or a result value of arbitrary type.
template <class Result>
using ServiceErrorOr = base::expected<Result, ServiceError>;

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_SERVICE_ERROR_H_
