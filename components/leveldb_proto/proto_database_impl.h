// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_IMPL_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_IMPL_H_

#include "components/leveldb_proto/unique_proto_database.h"

namespace leveldb_proto {

// Part of an ongoing effort to refactor ProtoDatabase. New users of
// ProtoDatabaseImpl should avoid using this in favor of including
// unique_proto_database.h and using UniqueProtoDatabase directly.
// See https://crbug.com/870813.
template <typename T>
using ProtoDatabaseImpl = UniqueProtoDatabase<T>;

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_IMPL_H_
