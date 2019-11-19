// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/download_store.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/proto/entry.pb.h"
#include "components/download/internal/background_service/proto_conversions.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace download {

namespace {

using KeyVector = std::vector<std::string>;
using ProtoEntryVector = std::vector<protodb::Entry>;
using KeyProtoEntryVector = std::vector<std::pair<std::string, protodb::Entry>>;

leveldb_env::Options GetDownloadDBOptions() {
  // These options reduce memory consumption.
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.reuse_logs = false;
  options.write_buffer_size = 64 << 10;  // 64 KiB
  return options;
}

}  // namespace

DownloadStore::DownloadStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<protodb::Entry>> db)
    : db_(std::move(db)), is_initialized_(false) {}

DownloadStore::~DownloadStore() = default;

bool DownloadStore::IsInitialized() {
  return is_initialized_;
}

void DownloadStore::Initialize(InitCallback callback) {
  DCHECK(!IsInitialized());
  db_->Init(GetDownloadDBOptions(),
            base::BindOnce(&DownloadStore::OnDatabaseInited,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadStore::HardRecover(StoreCallback callback) {
  is_initialized_ = false;
  db_->Destroy(base::BindOnce(&DownloadStore::OnDatabaseDestroyed,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadStore::OnDatabaseInited(InitCallback callback,
                                     leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    std::move(callback).Run(false, std::make_unique<std::vector<Entry>>());
    return;
  }

  db_->LoadEntries(base::BindOnce(&DownloadStore::OnDatabaseLoaded,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void DownloadStore::OnDatabaseLoaded(InitCallback callback,
                                     bool success,
                                     std::unique_ptr<ProtoEntryVector> protos) {
  if (!success) {
    std::move(callback).Run(success, std::make_unique<std::vector<Entry>>());
    return;
  }

  auto entries = ProtoConversions::EntryVectorFromProto(std::move(protos));
  is_initialized_ = true;
  std::move(callback).Run(success, std::move(entries));
}

void DownloadStore::OnDatabaseDestroyed(StoreCallback callback, bool success) {
  if (!success) {
    std::move(callback).Run(success);
    return;
  }

  db_->Init(GetDownloadDBOptions(),
            base::BindOnce(&DownloadStore::OnDatabaseInitedAfterDestroy,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadStore::OnDatabaseInitedAfterDestroy(
    StoreCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  is_initialized_ = status == leveldb_proto::Enums::InitStatus::kOK;
  std::move(callback).Run(is_initialized_);
}

void DownloadStore::Update(const Entry& entry, StoreCallback callback) {
  DCHECK(IsInitialized());
  auto entries_to_save = std::make_unique<KeyProtoEntryVector>();
  protodb::Entry proto = ProtoConversions::EntryToProto(entry);
  entries_to_save->emplace_back(entry.guid, std::move(proto));
  db_->UpdateEntries(std::move(entries_to_save), std::make_unique<KeyVector>(),
                     std::move(callback));
}

void DownloadStore::Remove(const std::string& guid, StoreCallback callback) {
  DCHECK(IsInitialized());
  auto keys_to_remove = std::make_unique<KeyVector>();
  keys_to_remove->push_back(guid);
  db_->UpdateEntries(std::make_unique<KeyProtoEntryVector>(),
                     std::move(keys_to_remove), std::move(callback));
}

}  // namespace download
