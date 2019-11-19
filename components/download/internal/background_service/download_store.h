// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_STORE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_STORE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/internal/background_service/store.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace protodb {
class Entry;
}  // namespace protodb

namespace download {

// DownloadStore provides a layer around a LevelDB proto database that persists
// the download request, scheduling params and metadata to the disk. The data is
// read during initialization and presented to the caller after converting to
// Entry entries.
class DownloadStore : public Store {
 public:
  DownloadStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<protodb::Entry>> db);
  ~DownloadStore() override;

  // Store implementation.
  bool IsInitialized() override;
  void Initialize(InitCallback callback) override;
  void HardRecover(StoreCallback callback) override;
  void Update(const Entry& entry, StoreCallback callback) override;
  void Remove(const std::string& guid, StoreCallback callback) override;

 private:
  void OnDatabaseInited(InitCallback callback,
                        leveldb_proto::Enums::InitStatus status);
  void OnDatabaseLoaded(InitCallback callback,
                        bool success,
                        std::unique_ptr<std::vector<protodb::Entry>> protos);
  void OnDatabaseDestroyed(StoreCallback callback, bool success);
  void OnDatabaseInitedAfterDestroy(StoreCallback callback,
                                    leveldb_proto::Enums::InitStatus status);

  std::unique_ptr<leveldb_proto::ProtoDatabase<protodb::Entry>> db_;
  bool is_initialized_;

  base::WeakPtrFactory<DownloadStore> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadStore);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_STORE_H_
