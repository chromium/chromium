// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/pending_file_set.h"

#include <stdint.h>

#include <algorithm>
#include <functional>

#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"

namespace sqlite_vfs {

PendingFileSet::PendingFileSet() = default;
PendingFileSet::PendingFileSet(PendingFileSet&&) = default;
PendingFileSet& PendingFileSet::operator=(PendingFileSet&&) = default;
PendingFileSet::~PendingFileSet() = default;

base::ByteSize PendingFileSet::ComputeTotalFileSize() const {
  base::ClampedNumeric<int64_t> total_size = 0;
  for (const base::File& file :
       {std::cref(db_file), std::cref(journal_file), std::cref(wal_file)}) {
    if (file.IsValid()) {
      // GetLength() returns a negative value on failure.
      total_size += std::max(file.GetLength(), int64_t{0});
    }
  }
  return base::ByteSize(base::checked_cast<uint64_t>(total_size.RawValue()));
}

bool PendingFileSet::IsNewDatabase() const {
  return db_file.created();
}

}  // namespace sqlite_vfs
