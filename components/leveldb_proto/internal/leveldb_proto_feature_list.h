// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_LEVELDB_PROTO_FEATURE_LIST_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_LEVELDB_PROTO_FEATURE_LIST_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace leveldb_proto {

extern const COMPONENT_EXPORT(LEVELDB_PROTO) base::Feature
    kProtoDBSharedMigration;

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_LEVELDB_PROTO_FEATURE_LIST_H_
