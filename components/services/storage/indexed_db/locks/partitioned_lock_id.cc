// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"

#include <iomanip>
#include <ostream>

namespace content::indexed_db {

std::ostream& operator<<(std::ostream& out, const PartitionedLockId& lock_id) {
  out << "<PartitionedLockId>{id: 0x";
  out << std::setfill('0');
  for (const char c : lock_id.key) {
    out << std::hex << std::setw(2) << static_cast<int>(c);
  }
  out << ", partition: " << lock_id.partition;
  out << "}";
  return out;
}

bool operator<(const PartitionedLockId& x, const PartitionedLockId& y) {
  if (x.partition != y.partition)
    return x.partition < y.partition;
  return x.key < y.key;
}

bool operator==(const PartitionedLockId& x, const PartitionedLockId& y) {
  return x.partition == y.partition && x.key == y.key;
}

bool operator!=(const PartitionedLockId& x, const PartitionedLockId& y) {
  return !(x == y);
}

}  // namespace content::indexed_db
