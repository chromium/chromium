// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_BACKEND_H_
#define COMPONENTS_PERSISTENT_CACHE_BACKEND_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/entry_metadata.h"

namespace persistent_cache {

class Entry;

// The persistence mechanism backing up the cache.
class COMPONENT_EXPORT(PERSISTENT_CACHE) Backend {
 public:
  virtual ~Backend();

  // Not copyable or moveable.
  Backend(const Backend&) = delete;
  Backend(Backend&&) = delete;
  Backend& operator=(const Backend&) = delete;
  Backend& operator=(Backend&&) = delete;

  // Initializes the cache. Must be called exactly once before any other
  // method. No further method can be called if this fails.
  virtual bool Initialize() = 0;

  // Used to get a handle to entry associated with `key`. Returns `nullptr` if
  // `key` is not found. Returned entry will remain valid and its contents will
  // be accessible for its entire lifetime. Note: Backends have to outlive
  // entries they vend.
  //
  // Thread-safe.
  virtual std::unique_ptr<Entry> Find(std::string_view key) = 0;

  // Used to add an entry containing `content` and associated with `key`.
  // Metadata associated with the entry can be provided in `metadata` or the
  // object can be default initialized to signify no metadata.
  //
  // This call will never report failure and `content` is expected (but not
  // guaranteed) to be resident upon return.
  //
  // Implementations are allowed to free other unused entries on demand to make
  // room or fail when full.
  //
  // Thread-safe.
  virtual void Insert(std::string_view key,
                      base::span<const uint8_t> content,
                      EntryMetadata metadata) = 0;

 protected:
  Backend();
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_BACKEND_H_
