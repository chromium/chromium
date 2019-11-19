// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/persistent_event_store.h"

#include <vector>

#include "base/bind.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace feature_engagement {
namespace {

using KeyEventPair = std::pair<std::string, Event>;
using KeyEventList = std::vector<KeyEventPair>;

void NoopUpdateCallback(bool success) {
  stats::RecordDbUpdate(success, stats::StoreType::EVENTS_STORE);
}

}  // namespace

PersistentEventStore::PersistentEventStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<Event>> db)
    : db_(std::move(db)), ready_(false) {}

PersistentEventStore::~PersistentEventStore() = default;

void PersistentEventStore::Load(const OnLoadedCallback& callback) {
  DCHECK(!ready_);

  db_->Init(base::BindOnce(&PersistentEventStore::OnInitComplete,
                           weak_ptr_factory_.GetWeakPtr(), callback));
}

bool PersistentEventStore::IsReady() const {
  return ready_;
}

void PersistentEventStore::WriteEvent(const Event& event) {
  DCHECK(IsReady());

  std::unique_ptr<KeyEventList> entries = std::make_unique<KeyEventList>();
  entries->push_back(KeyEventPair(event.name(), event));

  db_->UpdateEntries(std::move(entries),
                     std::make_unique<std::vector<std::string>>(),
                     base::BindOnce(&NoopUpdateCallback));
}

void PersistentEventStore::DeleteEvent(const std::string& event_name) {
  DCHECK(IsReady());
  auto deletes = std::make_unique<std::vector<std::string>>();
  deletes->push_back(event_name);

  db_->UpdateEntries(std::make_unique<KeyEventList>(), std::move(deletes),
                     base::BindOnce(&NoopUpdateCallback));
}

void PersistentEventStore::OnInitComplete(
    const OnLoadedCallback& callback,
    leveldb_proto::Enums::InitStatus status) {
  bool success = status == leveldb_proto::Enums::InitStatus::kOK;
  stats::RecordDbInitEvent(success, stats::StoreType::EVENTS_STORE);

  if (!success) {
    callback.Run(false, std::make_unique<std::vector<Event>>());
    return;
  }

  db_->LoadEntries(base::BindOnce(&PersistentEventStore::OnLoadComplete,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
}

void PersistentEventStore::OnLoadComplete(
    const OnLoadedCallback& callback,
    bool success,
    std::unique_ptr<std::vector<Event>> entries) {
  stats::RecordEventDbLoadEvent(success, *entries);
  ready_ = success;
  callback.Run(success, std::move(entries));
}

}  // namespace feature_engagement
