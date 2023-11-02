// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_store.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace video_tutorials {
namespace {
// The key for the only database entry in the video tutorials database.
constexpr char kDatabaseEntryKey[] = "";

}  // namespace

TutorialStore::TutorialStore(TutorialProtoDb db) : db_(std::move(db)) {}

TutorialStore::~TutorialStore() = default;

void TutorialStore::InitAndLoad(LoadCallback callback) {
  bool initialized = init_success_.has_value() && init_success_.value();
  if (initialized) {
    db_->GetEntry(kDatabaseEntryKey, std::move(callback));
    return;
  }

  db_->Init(base::BindOnce(&TutorialStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void TutorialStore::Update(const proto::VideoTutorialGroups& entry,
                           UpdateCallback callback) {
  using KeyEntryVector =
      std::vector<std::pair<std::string, proto::VideoTutorialGroups>>;
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  entries_to_save->emplace_back(kDatabaseEntryKey, entry);

  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     std::move(callback));
}

void TutorialStore::OnDbInitialized(LoadCallback callback,
                                    leveldb_proto::Enums::InitStatus status) {
  init_success_ = status == leveldb_proto::Enums::InitStatus::kOK;
  if (!init_success_) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  db_->GetEntry(kDatabaseEntryKey, std::move(callback));
}

}  // namespace video_tutorials
