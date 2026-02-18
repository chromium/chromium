// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_STATUS_HELPER_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_STATUS_HELPER_H_

#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {

class DbStatus;
// A set of helpers to convert between `storage::DbStatus` and
// `leveldb::Status`, and to access some leveldb::Status utilities with a
// `storage::DbStatus`.
DbStatus FromLevelDBStatus(const leveldb::Status& status);

leveldb::Status ToLevelDBStatus(const DbStatus& status);

void LogLevelDBStatusHistogram(std::string_view histogram_name,
                               const DbStatus& status);

bool IndicatesDiskFull(const DbStatus& status);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_STATUS_HELPER_H_
