// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_backend.h"

#include "components/power_bookmarks/storage/empty_power_bookmark_database.h"
#include "components/power_bookmarks/storage/power_bookmark_database_impl.h"

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
    db_ = std::make_unique<PowerBookmarkDatabaseImpl>(database_dir_);
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
    const PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetPowersForURL(url, power_type);
}

std::vector<std::unique_ptr<PowerOverview>>
PowerBookmarkBackend::GetPowerOverviewsForType(const PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetPowerOverviewsForType(power_type);
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

bool PowerBookmarkBackend::DeletePowersForURL(const GURL& url,
                                              const PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->DeletePowersForURL(url, power_type);
}

}  // namespace power_bookmarks
