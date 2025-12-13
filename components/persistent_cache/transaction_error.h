// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_TRANSACTION_ERROR_H_
#define COMPONENTS_PERSISTENT_CACHE_TRANSACTION_ERROR_H_

namespace persistent_cache {

enum class TransactionError {
  // Transient backend error. (ex: Busy)
  kTransient,

  // The connection to the backend has encountered an error. A retry with a new
  // connection may resolve the issue.
  kConnectionError,

  // Backend is permanently unusable. Files should be deleted. (ex:
  // Initialization failed)
  kPermanent,
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_TRANSACTION_ERROR_H_
