// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_store.h"

#include <utility>

#include "base/logging.h"
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

void TutorialStore::Initialize(SuccessCallback callback) {
  db_->Init(base::BindOnce(&TutorialStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void TutorialStore::LoadEntries(const std::vector<std::string>& keys,
                                LoadEntriesCallback callback) {
  if (keys.empty()) {
    // Load all entries.
    db_->LoadEntries(std::move(callback));
    return;
  }

  db_->LoadEntriesWithFilter(
      base::BindRepeating(
          [](const std::vector<std::string>& key_dict, const std::string& key) {
            return base::Contains(key_dict, key);
          },
          keys),
      std::move(callback));
}

void TutorialStore::UpdateAll(
    const std::vector<std::pair<std::string, TutorialGroup>>& key_entry_pairs,
    const std::vector<std::string>& keys_to_delete,
    UpdateCallback callback) {
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  for (auto& pair : key_entry_pairs)
    entries_to_save->emplace_back(pair.first, pair.second);

  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  for (auto key : keys_to_delete)
    keys_to_remove->emplace_back(key);

  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     std::move(callback));
}

void TutorialStore::OnDbInitialized(SuccessCallback callback,
                                    leveldb_proto::Enums::InitStatus status) {
  std::move(callback).Run(status == leveldb_proto::Enums::InitStatus::kOK);
}

}  // namespace video_tutorials
