// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_SCOPE_LOCK_RANGE_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_SCOPE_LOCK_RANGE_H_

#include <stdint.h>
#include <iosfwd>
#include <vector>

#include "base/logging.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content {

// The range is [begin, end). Bytewise comparison is used to determine
// overlapping ranges.
struct ScopeLockRange {
  ScopeLockRange() = default;
  ~ScopeLockRange() = default;
  std::string begin;
  std::string end;

  bool IsValid() const { return begin < end; }
};

// Logging support.
std::ostream& operator<<(std::ostream& out, const ScopeLockRange& range);

bool operator<(const ScopeLockRange& x, const ScopeLockRange& y);
bool operator==(const ScopeLockRange& x, const ScopeLockRange& y);
bool operator!=(const ScopeLockRange& x, const ScopeLockRange& y);

}  // namespace content

#endif /* COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_SCOPE_LOCK_RANGE_H_ */
