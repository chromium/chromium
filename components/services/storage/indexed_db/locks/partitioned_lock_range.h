// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_RANGE_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_RANGE_H_

#include <stdint.h>
#include <iosfwd>
#include <string>

#include "base/component_export.h"

namespace content {

// The range is [begin, end). Bytewise comparison is used to determine
// overlapping ranges.
struct COMPONENT_EXPORT(LOCK_MANAGER) PartitionedLockRange {
  std::string begin;
  std::string end;

  bool IsValid() const { return begin < end; }
};

// Logging support.
COMPONENT_EXPORT(LOCK_MANAGER)
std::ostream& operator<<(std::ostream& out, const PartitionedLockRange& range);

COMPONENT_EXPORT(LOCK_MANAGER)
bool operator<(const PartitionedLockRange& x, const PartitionedLockRange& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator==(const PartitionedLockRange& x, const PartitionedLockRange& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator!=(const PartitionedLockRange& x, const PartitionedLockRange& y);

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_RANGE_H_
