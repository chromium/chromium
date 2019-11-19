// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPE_DELETION_MODE_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPE_DELETION_MODE_H_

enum class LevelDBScopeDeletionMode {
  // The range is deleted immediately.
  // This mode treats the 'end' of the range as exclusive.
  kImmediateWithRangeEndExclusive,
  // The range is deleted immediately, where it will be iterated and deleted.
  // This mode treats the 'end' of the range as inclusive.
  kImmediateWithRangeEndInclusive,
  // The range will be deleted eventually. All future access to this range is
  // undefined - the data may or may not still exist, even after Chrome
  // restarts. So only do this for ranges that are known to never be used again.
  // This mode treats the 'end' of the range as exclusive.
  kDeferred,
  // Same as |kDeferred|, except the range will also be compacted afterwards
  // to further ensure the data is deleted.
  // This mode treats the 'end' of the range as exclusive.
  kDeferredWithCompaction
};

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPE_DELETION_MODE_H_
