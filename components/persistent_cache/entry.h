// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_ENTRY_H_
#define COMPONENTS_PERSISTENT_CACHE_ENTRY_H_

#include <cstdint>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/persistent_cache/entry_metadata.h"

namespace persistent_cache {

// Provides access to a snapshot of the content of an entry. The snapshot
// remains valid and unmodified even if the entry is modified or removed in the
// cache.
class COMPONENT_EXPORT(PERSISTENT_CACHE) Entry {
 public:
  virtual ~Entry() = 0;

  // Not copyable or moveable.
  Entry(const Entry&) = delete;
  Entry(Entry&&) = delete;
  Entry& operator=(const Entry&) = delete;
  Entry& operator=(Entry&&) = delete;

  // Use to acquire a span that is kept valid until this Entry is released. If
  // looking to immediately copy contents prefer `CopyContentTo` which is
  // guaranteed to be equally or more performant.
  virtual base::span<const uint8_t> GetContentSpan() const = 0;

  // Use to copy the content of the entry to `content`.
  virtual size_t CopyContentTo(base::span<uint8_t> content) const;

  // Use to get the size of the entry's value in bytes.
  virtual size_t GetContentSize() const;

  // Use to retrieve metadata tied to the entry. Partially or completely
  // populated by default values if the metadata was not supplied on insert.
  virtual EntryMetadata GetMetadata() const = 0;

 protected:
  // Ownership and liveness is managed by the backend and thus Entry should not
  // be created outside of their scope.
  Entry() = default;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_ENTRY_H_
