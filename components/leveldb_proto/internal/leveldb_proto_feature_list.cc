// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"

namespace leveldb_proto {

BASE_FEATURE(kProtoDBSharedMigration,
             "ProtoDBSharedMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace leveldb_proto
