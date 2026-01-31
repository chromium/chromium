// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb_utils.h"

namespace storage {

DomStorageDatabase::Key CreatePrefixedKey(
    const DomStorageDatabase::KeyView& prefix,
    const DomStorageDatabase::KeyView& key) {
  DomStorageDatabase::Key result;
  result.reserve(prefix.size() + key.size());

  // Append `prefix`.
  result.insert(result.end(), prefix.begin(), prefix.end());

  // Append `key`.
  result.insert(result.end(), key.begin(), key.end());
  return result;
}

}  // namespace storage
