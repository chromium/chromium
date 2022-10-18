// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_ID_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_ID_H_

#include <iosfwd>
#include <string>

#include "base/component_export.h"

namespace content {

struct COMPONENT_EXPORT(LOCK_MANAGER) PartitionedLockId {
  int partition;
  std::string key;
};

// Logging support.
COMPONENT_EXPORT(LOCK_MANAGER)
std::ostream& operator<<(std::ostream& out, const PartitionedLockId& range);

COMPONENT_EXPORT(LOCK_MANAGER)
bool operator<(const PartitionedLockId& x, const PartitionedLockId& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator==(const PartitionedLockId& x, const PartitionedLockId& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator!=(const PartitionedLockId& x, const PartitionedLockId& y);

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_ID_H_
