// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb_status_helper.h"

#include "components/services/storage/dom_storage/db_status.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {

DbStatus FromLevelDBStatus(const leveldb::Status& status) {
  if (status.ok()) {
    return DbStatus::OK();
  } else if (status.IsNotFound()) {
    return DbStatus::NotFound(status.ToString());
  } else if (status.IsCorruption()) {
    return DbStatus::Corruption(status.ToString());
  } else if (status.IsNotSupportedError()) {
    return DbStatus::NotSupported(status.ToString());
  } else if (status.IsIOError()) {
    return DbStatus::IOError(status.ToString());
  } else {
    return DbStatus::InvalidArgument(status.ToString());
  }
}

}  // namespace storage
