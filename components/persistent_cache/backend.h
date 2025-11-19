// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_BACKEND_H_
#define COMPONENTS_PERSISTENT_CACHE_BACKEND_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/persistent_cache/buffer_provider.h"
#include "components/persistent_cache/entry_metadata.h"
#include "components/persistent_cache/lock_state.h"
#include "components/persistent_cache/transaction_error.h"

namespace persistent_cache {

enum class BackendType;

// The persistence mechanism backing up the cache.
class COMPONENT_EXPORT(PERSISTENT_CACHE) Backend {
 public:
  virtual ~Backend();

  // Not copyable or moveable.
  Backend(const Backend&) = delete;
  Backend(Backend&&) = delete;
  Backend& operator=(const Backend&) = delete;
  Backend& operator=(Backend&&) = delete;

  // See `PersistentCache::Find()`.
  // Note: Backends have to outlive entries they vend.
  //
  // Thread-safe.
  virtual base::expected<std::optional<EntryMetadata>, TransactionError> Find(
      std::string_view key,
      BufferProvider buffer_provider) = 0;

  // See `PersistentCache::Insert()`.
  // Thread-safe.
  virtual base::expected<void, TransactionError> Insert(
      std::string_view key,
      base::span<const uint8_t> content,
      EntryMetadata metadata) = 0;

  // Used to get type of instance. Intended for things like metrics recording.
  // Externally behavior of all backend types should be equivalent and control
  // flow should not be tailored to the type.
  virtual BackendType GetType() const = 0;

  // Used to understand if the instance has read only access. Intended for
  // things like metrics recording. Externally behavior of all backend types
  // should be equivalent for reads. Writes should probably not be attempted if
  // not permitted.
  virtual bool IsReadOnly() const = 0;

  // See `PersistentCache::Abandon()` documentation.
  virtual LockState Abandon() = 0;

 protected:
  Backend();
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_BACKEND_H_
