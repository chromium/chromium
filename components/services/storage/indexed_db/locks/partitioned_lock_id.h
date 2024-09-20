// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_ID_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_ID_H_

#include <iosfwd>
#include <string>

namespace content::indexed_db {

struct PartitionedLockId {
  int partition;
  std::string key;
};

// Logging support.
std::ostream& operator<<(std::ostream& out, const PartitionedLockId& range);

bool operator<(const PartitionedLockId& x, const PartitionedLockId& y);
bool operator==(const PartitionedLockId& x, const PartitionedLockId& y);
bool operator!=(const PartitionedLockId& x, const PartitionedLockId& y);

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_ID_H_
