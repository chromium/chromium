// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/leveled_lock_range.h"

#include <iomanip>
#include <ostream>

namespace content {

std::ostream& operator<<(std::ostream& out, const LeveledLockRange& range) {
  out << "<LeveledLockRange>{begin: 0x";
  out << std::setfill('0');
  for (const char c : range.begin) {
    out << std::hex << std::setw(2) << static_cast<int>(c);
  }
  out << ", end: 0x";
  for (const char c : range.end) {
    out << std::hex << std::setw(2) << static_cast<int>(c);
  }
  out << "}";
  return out;
}

bool operator<(const LeveledLockRange& x, const LeveledLockRange& y) {
  if (x.begin != y.begin)
    return x.begin < y.begin;
  return x.end < y.end;
}

bool operator==(const LeveledLockRange& x, const LeveledLockRange& y) {
  return x.begin == y.begin && x.end == y.end;
}
bool operator!=(const LeveledLockRange& x, const LeveledLockRange& y) {
  return !(x == y);
}

}  // namespace content
