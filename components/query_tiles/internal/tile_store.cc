// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_store.h"

#include <utility>

#include "components/query_tiles/internal/proto_conversion.h"

namespace leveldb_proto {

void DataToProto(query_tiles::TileGroup* data,
                 query_tiles::proto::TileGroup* proto) {
  TileGroupToProto(data, proto);
}

void ProtoToData(query_tiles::proto::TileGroup* proto,
                 query_tiles::TileGroup* data) {
  TileGroupFromProto(proto, data);
}

}  // namespace leveldb_proto

namespace query_tiles {

TileStore::TileStore(TileProtoDb db) : db_(std::move(db)) {}

TileStore::~TileStore() = default;

void TileStore::InitAndLoad(LoadCallback callback) {
  db_->Init(base::BindOnce(&TileStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void TileStore::Update(const std::string& key,
                       const TileGroup& group,
                       UpdateCallback callback) {
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  TileGroup entry_to_save = group;
  entries_to_save->emplace_back(key, std::move(entry_to_save));
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<KeyVector>() /*keys_to_remove*/,
                     std::move(callback));
}

void TileStore::Delete(const std::string& key, DeleteCallback callback) {
  auto keys_to_delete = std::make_unique<KeyVector>();
  keys_to_delete->emplace_back(key);
  db_->UpdateEntries(std::make_unique<KeyEntryVector>() /*entries_to_save*/,
                     std::move(keys_to_delete), std::move(callback));
}

void TileStore::OnDbInitialized(LoadCallback callback,
                                leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    std::move(callback).Run(false, KeysAndEntries());
    return;
  }

  db_->LoadKeysAndEntries(base::BindOnce(&TileStore::OnDataLoaded,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback)));
}

void TileStore::OnDataLoaded(
    LoadCallback callback,
    bool success,
    std::unique_ptr<std::map<std::string, TileGroup>> loaded_keys_and_entries) {
  if (!success || !loaded_keys_and_entries) {
    std::move(callback).Run(success, KeysAndEntries());
    return;
  }

  KeysAndEntries keys_and_entries;
  for (auto& it : *loaded_keys_and_entries) {
    std::unique_ptr<TileGroup> group =
        std::make_unique<TileGroup>(std::move(it.second));
    keys_and_entries.emplace(it.first, std::move(group));
  }

  std::move(callback).Run(true, std::move(keys_and_entries));
}

}  // namespace query_tiles
