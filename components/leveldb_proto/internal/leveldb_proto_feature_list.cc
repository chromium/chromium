// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"

namespace leveldb_proto {

BASE_FEATURE(kProtoDBSharedMigration, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether database writes are asynchronous. This reduces disk
// contention and improves overall browser speed. The last asynchronous writes
// may be lost in case of operating system or power failure (note: a mere
// process crash wouldn't prevent a write from completing), but leveldb_proto
// clients don't have strong persistence requirements (see
// https://docs.google.com/document/d/1nd74W_uUZrU0sOFjWO9xyxFhQPIR1uBcJyoRWkw0_LA/edit?usp=sharing).
// Database corruption is not a concern due to leveldb's journaling system. More
// details at
// https://github.com/google/leveldb/blob/main/doc/index.md#synchronous-writes.
BASE_FEATURE(kLevelDBProtoAsyncWrite, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace leveldb_proto
