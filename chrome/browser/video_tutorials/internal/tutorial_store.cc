// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_store.h"

#include <utility>

#include "base/stl_util.h"

namespace leveldb_proto {

void DataToProto(video_tutorials::TutorialGroup* data,
                 video_tutorials::proto::VideoTutorialGroup* proto) {
  TutorialGroupToProto(data, proto);
}

void ProtoToData(video_tutorials::proto::VideoTutorialGroup* proto,
                 video_tutorials::TutorialGroup* data) {
  TutorialGroupFromProto(proto, data);
}

}  // namespace leveldb_proto

namespace video_tutorials {

TutorialStore::TutorialStore(TutorialProtoDb db) : db_(std::move(db)) {}

TutorialStore::~TutorialStore() = default;

void TutorialStore::InitAndLoadKeys(LoadKeysCallback callback) {
  db_->Init(base::BindOnce(&TutorialStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void TutorialStore::LoadEntries(const std::vector<std::string>& keys,
                                LoadEntriesCallback callback) {
  db_->LoadEntriesWithFilter(
      base::BindRepeating(
          [](const std::vector<std::string>& key_dict, const std::string& key) {
            return base::Contains(key_dict, key);
          },
          keys),
      base::BindOnce(&TutorialStore::OnEntriesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TutorialStore::Update(const std::string& key,
                           const TutorialGroup& group,
                           UpdateCallback callback) {
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  auto entry_to_save = group;
  entries_to_save->emplace_back(key, std::move(entry_to_save));
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<KeyVector>() /*keys_to_remove*/,
                     std::move(callback));
}

void TutorialStore::Delete(const std::vector<std::string>& keys,
                           DeleteCallback callback) {
  auto keys_to_delete = std::make_unique<KeyVector>(keys);
  db_->UpdateEntries(std::make_unique<KeyEntryVector>() /*entries_to_save*/,
                     std::move(keys_to_delete), std::move(callback));
}

void TutorialStore::OnDbInitialized(LoadKeysCallback callback,
                                    leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    std::move(callback).Run(false, Keys());
    return;
  }

  db_->LoadKeys(base::BindOnce(&TutorialStore::OnKeysLoaded,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback)));
}

void TutorialStore::OnKeysLoaded(LoadKeysCallback callback,
                                 bool success,
                                 std::unique_ptr<KeyVector> keys) {
  std::move(callback).Run(success, success ? std::move(keys) : Keys());
}

void TutorialStore::OnEntriesLoaded(
    LoadEntriesCallback callback,
    bool success,
    std::unique_ptr<std::vector<TutorialGroup>> loaded_entries) {
  if (!success || !loaded_entries) {
    std::move(callback).Run(success, Entries());
    return;
  }
  Entries entries;
  for (auto& loaded_entry : *loaded_entries)
    entries.emplace_back(std::make_unique<TutorialGroup>(loaded_entry));
  std::move(callback).Run(true, std::move(entries));
}

}  // namespace video_tutorials
