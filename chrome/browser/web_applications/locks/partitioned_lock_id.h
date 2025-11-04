// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_ID_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_ID_H_

#include <iosfwd>
#include <string>

namespace web_app {

struct PartitionedLockId {
  int partition;
  std::string key;

  friend auto operator<=>(const PartitionedLockId&,
                          const PartitionedLockId&) = default;
  friend bool operator==(const PartitionedLockId&,
                         const PartitionedLockId&) = default;

  template <typename H>
  friend H AbslHashValue(H h, const PartitionedLockId& m) {
    return H::combine(std::move(h), m.partition, m.key);
  }
};

// Logging support.
std::ostream& operator<<(std::ostream& out, const PartitionedLockId& range);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_ID_H_
