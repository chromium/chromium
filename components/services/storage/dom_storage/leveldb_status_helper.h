// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_STATUS_HELPER_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_STATUS_HELPER_H_

#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {

class DbStatus;

// Creates a `storage::DbStatus` using the leveldb::Status's type and error
// message.
DbStatus FromLevelDBStatus(const leveldb::Status& status);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_STATUS_HELPER_H_
