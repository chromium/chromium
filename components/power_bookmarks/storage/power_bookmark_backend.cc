// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_backend.h"

#include "components/power_bookmarks/core/powers/search_params.h"
#include "components/power_bookmarks/storage/empty_power_bookmark_database.h"
#include "components/power_bookmarks/storage/power_bookmark_database_impl.h"
#include "components/power_bookmarks/storage/power_bookmark_sync_bridge.h"
#include "components/power_bookmarks/storage/power_bookmark_sync_metadata_database.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"

namespace power_bookmarks {

PowerBookmarkBackend::PowerBookmarkBackend(const base::FilePath& database_dir)
    : database_dir_(database_dir) {
  // This is constructed on the browser thread, but all other interactions
  // happen on a background thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PowerBookmarkBackend::~PowerBookmarkBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PowerBookmarkBackend::Init(bool use_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.reset();

  // Substitute a dummy implementation when the feature is disabled.
  if (use_database) {
    auto database = std::make_unique<PowerBookmarkDatabaseImpl>(database_dir_);

    // TODO(crbug.com/1392502): Plumb in syncer::ReportUnrecoverableError as the
    // dump_stack callback.
    auto change_processor =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::POWER_BOOKMARK, /*dump_stack=*/base::RepeatingClosure());
    bridge_ = std::make_unique<PowerBookmarkSyncBridge>(
        database->GetSyncMetadataDatabase(), database.get(),
        std::move(change_processor));
    db_ = std::move(database);
  } else {
    db_ = std::make_unique<EmptyPowerBookmarkDatabase>();
  }

  bool success = db_->Init();
  DCHECK(success);
}

void PowerBookmarkBackend::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.reset();
}

std::vector<std::unique_ptr<Power>> PowerBookmarkBackend::GetPowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetPowersForURL(url, power_type);
}

std::vector<std::unique_ptr<PowerOverview>>
PowerBookmarkBackend::GetPowerOverviewsForType(
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetPowerOverviewsForType(power_type);
}

std::vector<std::unique_ptr<Power>> PowerBookmarkBackend::Search(
    const SearchParams& search_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetPowersForSearchParams(search_params);
}

bool PowerBookmarkBackend::CreatePower(std::unique_ptr<Power> power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->CreatePower(std::move(power));
}

bool PowerBookmarkBackend::UpdatePower(std::unique_ptr<Power> power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->UpdatePower(std::move(power));
}

bool PowerBookmarkBackend::DeletePower(const base::GUID& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->DeletePower(guid);
}

bool PowerBookmarkBackend::DeletePowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->DeletePowersForURL(url, power_type);
}

}  // namespace power_bookmarks
