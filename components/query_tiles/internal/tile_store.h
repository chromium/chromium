// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_STORE_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/query_tiles/internal/store.h"
#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/proto/tile.pb.h"
#include "components/query_tiles/tile.h"

namespace leveldb_proto {

void DataToProto(query_tiles::TileGroup* data,
                 query_tiles::proto::TileGroup* proto);
void ProtoToData(query_tiles::proto::TileGroup* proto,
                 query_tiles::TileGroup* data);

}  // namespace leveldb_proto

namespace query_tiles {
// TileStore is the storage layer of all TileGroup which contains
// the top-level tile entries and group metadata. Sub-level tiles are
// recursively owned by their parents.
class TileStore : public Store<TileGroup> {
 public:
  using TileProtoDb = std::unique_ptr<
      leveldb_proto::ProtoDatabase<query_tiles::proto::TileGroup, TileGroup>>;
  explicit TileStore(TileProtoDb db);
  ~TileStore() override;

  TileStore(const TileStore& other) = delete;
  TileStore& operator=(const TileStore& other) = delete;

 private:
  using KeyEntryVector = std::vector<std::pair<std::string, TileGroup>>;
  using KeyVector = std::vector<std::string>;
  using EntryVector = std::vector<TileGroup>;

  // Store<TileGroup> implementation.
  void InitAndLoad(LoadCallback callback) override;
  void Update(const std::string& key,
              const TileGroup& entry,
              UpdateCallback callback) override;
  void Delete(const std::string& key, DeleteCallback callback) override;

  // Called when db is initialized.
  void OnDbInitialized(LoadCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  // Called when keys and entries are loaded from db.
  void OnDataLoaded(
      LoadCallback callback,
      bool success,
      std::unique_ptr<std::map<std::string, TileGroup>> loaded_entries);

  TileProtoDb db_;

  base::WeakPtrFactory<TileStore> weak_ptr_factory_{this};
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_STORE_H_
