// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_backend.h"

#include "base/task/sequenced_task_runner.h"
#include "components/power_bookmarks/common/power_bookmark_metrics.h"
#include "components/power_bookmarks/common/search_params.h"
#include "components/power_bookmarks/storage/empty_power_bookmark_database.h"
#include "components/power_bookmarks/storage/power_bookmark_database_impl.h"

namespace power_bookmarks {

PowerBookmarkBackend::PowerBookmarkBackend(
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> frontend_task_runner,
    base::WeakPtr<PowerBookmarkObserver> service_observer)
    : database_dir_(database_dir),
      frontend_task_runner_(frontend_task_runner),
      service_observer_(service_observer) {
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

  // Substitute a dummy implementation when the feature is disabled. Note that
  // `use_database` is the PowerBookmarkBackend feature toggle on the call site.
  if (use_database) {
    db_ = std::make_unique<PowerBookmarkDatabaseImpl>(database_dir_);
    db_->Init();
  } else {
    db_ = std::make_unique<EmptyPowerBookmarkDatabase>();
    bool success = db_->Init();
    DCHECK(success);
  }
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

std::vector<std::unique_ptr<Power>> PowerBookmarkBackend::SearchPowers(
    const SearchParams& search_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetPowersForSearchParams(search_params);
}

std::vector<std::unique_ptr<PowerOverview>>
PowerBookmarkBackend::SearchPowerOverviews(const SearchParams& search_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetPowerOverviewsForSearchParams(search_params);
}

bool PowerBookmarkBackend::CreatePower(std::unique_ptr<Power> power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto transaction = db_->BeginTransaction();
  if (!transaction) {
    return false;
  }
  sync_pb::PowerBookmarkSpecifics::PowerType power_type = power->power_type();
  bool success = db_->CreatePower(power->Clone());
  metrics::RecordPowerCreated(power_type, success);
  if (!success) {
    return false;
  }
  return CommitAndNotify(*transaction);
}

bool PowerBookmarkBackend::UpdatePower(std::unique_ptr<Power> power) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto transaction = db_->BeginTransaction();
  if (!transaction) {
    return false;
  }
  sync_pb::PowerBookmarkSpecifics::PowerType power_type = power->power_type();
  auto updated_power = db_->UpdatePower(std::move(power));
  bool success = updated_power != nullptr;
  metrics::RecordPowerUpdated(power_type, success);
  if (!success) {
    return false;
  }
  return CommitAndNotify(*transaction);
}

bool PowerBookmarkBackend::DeletePower(const base::Uuid& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto transaction = db_->BeginTransaction();
  if (!transaction) {
    return false;
  }
  bool success = db_->DeletePower(guid);
  metrics::RecordPowerDeleted(success);
  if (!success) {
    return false;
  }
  return CommitAndNotify(*transaction);
}

bool PowerBookmarkBackend::DeletePowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto transaction = db_->BeginTransaction();
  if (!transaction) {
    return false;
  }
  std::vector<std::string> deleted_guids;
  bool success = db_->DeletePowersForURL(url, power_type, &deleted_guids);
  metrics::RecordPowersDeletedForURL(power_type, success);
  if (!success) {
    return false;
  }
  return CommitAndNotify(*transaction);
}

bool PowerBookmarkBackend::CommitAndNotify(Transaction& transaction) {
  if (transaction.Commit()) {
    NotifyPowersChanged();
    return true;
  } else {
    return false;
  }
}

void PowerBookmarkBackend::NotifyPowersChanged() {
  // TODO(crbug.com/40252685): Posting a task here causes the observer method
  // to be called before the callback. This behavior is pretty strange, but
  // not a problem right now. Eventually we should stop using SequenceBound
  // for the backend and post tasks directly to ensure proper ordering.
  frontend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PowerBookmarkObserver::OnPowersChanged,
                                service_observer_));
}

}  // namespace power_bookmarks
