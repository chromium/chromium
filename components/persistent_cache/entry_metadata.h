// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_ENTRY_METADATA_H_
#define COMPONENTS_PERSISTENT_CACHE_ENTRY_METADATA_H_

#include "base/component_export.h"
#include "base/time/time.h"

namespace persistent_cache {

// Metadata that can be optionally attached to an `Entry`. Leaving
// some or all members to their default value is always valid. EntryMetadata is
// not a replacement or an enhancement to either keys or values. It represents
// information that aids the internal operations of the cache such as
// eviction. In some cases the data might be of some use to code outside of
// persistent_cache. As such it is externally accessible through `Entry`.
// Attempts to use metadata as additional storage for arbitrary values may
// result in performance degradation or unexpected behavior.
struct COMPONENT_EXPORT(PERSISTENT_CACHE) EntryMetadata {
  // Opaque token indicating whether the cached entry is still valid
  int64_t input_signature = 0;

  // Time at which the entry was inserted into the cache.
  int64_t write_timestamp = 0;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_ENTRY_METADATA_H_
