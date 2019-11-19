// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_MIGRATION_DELEGATE_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_MIGRATION_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/internal/unique_proto_database.h"

namespace leveldb_proto {

// Small class that strictly performs the migration functionality of
// SharedProtoDatabase.
class MigrationDelegate {
 public:
  using MigrationCallback = base::OnceCallback<void(bool)>;

  MigrationDelegate();
  ~MigrationDelegate();

  // Copies the keys/entries of |from| to |to|, and returns a boolean success
  // in |callback|.
  void DoMigration(UniqueProtoDatabase* from,
                   UniqueProtoDatabase* to,
                   MigrationCallback callback);

 private:
  void OnLoadKeysAndEntries(MigrationCallback callback,
                            UniqueProtoDatabase* to,
                            bool success,
                            std::unique_ptr<KeyValueMap> keys_entries);
  void OnUpdateEntries(MigrationCallback callback, bool success);

  base::WeakPtrFactory<MigrationDelegate> weak_ptr_factory_{this};
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_MIGRATION_DELEGATE_H_