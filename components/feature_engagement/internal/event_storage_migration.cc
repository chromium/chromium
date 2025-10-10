// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/event_storage_migration.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/feature_engagement/internal/event_model_impl.h"

namespace feature_engagement {

namespace {
// Type alias for a pair of a key and an event.
using KeyEventPair = std::pair<std::string, Event>;
// Type alias for a list of key and event pairs.
using KeyEventList = std::vector<KeyEventPair>;
}  // namespace

EventStorageMigration::EventStorageMigration(
    raw_ptr<leveldb_proto::ProtoDatabase<Event>> profile_db,
    raw_ptr<leveldb_proto::ProtoDatabase<Event>> device_db,
    raw_ptr<EventModelImpl> device_event_model)
    : profile_db_(profile_db),
      device_db_(device_db),
      device_event_model_(device_event_model) {}

EventStorageMigration::~EventStorageMigration() = default;

void EventStorageMigration::Migrate(MigrationCallback callback,
                                    uint32_t current_day) {
  // If a request is already in progress, drop the new request.
  if (migration_callback_) {
    return;
  }

  RecordMigrationStatus(EventStorageMigrationStatus::kStarted);

  // Set the callback to be invoked once overall initialization is complete.
  migration_callback_ = std::move(callback);
  profile_db_->LoadEntries(
      base::BindOnce(&EventStorageMigration::OnLoadEntriesComplete,
                     weak_ptr_factory_.GetWeakPtr(), current_day));
}

void EventStorageMigration::OnLoadEntriesComplete(
    uint32_t current_day,
    bool success,
    std::unique_ptr<std::vector<Event>> entries) {
  if (!success) {
    RecordMigrationStatus(EventStorageMigrationStatus::kFailedToLoad);
    std::move(migration_callback_).Run(false);
    return;
  }

  // Read profile events into a list.
  std::unique_ptr<KeyEventList> event_list = std::make_unique<KeyEventList>();
  for (const auto& event : *entries) {
    event_list->push_back(KeyEventPair(event.name(), event));
  }

  // Write profile to device db.
  device_db_->UpdateEntries(
      std::move(event_list), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&EventStorageMigration::OnEventWrittenCompleted,
                     weak_ptr_factory_.GetWeakPtr(), current_day,
                     std::move(entries)));
}

void EventStorageMigration::OnEventWrittenCompleted(
    uint32_t current_day,
    std::unique_ptr<std::vector<Event>> entries,
    bool success) {
  if (!success) {
    RecordMigrationStatus(EventStorageMigrationStatus::kFailedToWrite);
    std::move(migration_callback_).Run(false);
    return;
  }

  // Update the in-memory device event model.
  device_event_model_->OnStoreLoaded(
      base::BindOnce(&EventStorageMigration::OnDeviceEventModelUpdateCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      current_day, success, std::move(entries));
}

void EventStorageMigration::OnDeviceEventModelUpdateCompleted(bool success) {
  RecordMigrationStatus(success ? EventStorageMigrationStatus::kCompleted
                                : EventStorageMigrationStatus::kFailedToWrite);

  std::move(migration_callback_).Run(success);
}

// static
void EventStorageMigration::RecordMigrationStatus(
    EventStorageMigrationStatus status) {
  base::UmaHistogramEnumeration("InProductHelp.EventStorageMigration.Status",
                                status);
}

}  // namespace feature_engagement
