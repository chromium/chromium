// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/database/ukm_database_impl.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "components/segmentation_platform/internal/signals/ukm_observer.h"
#include "components/segmentation_platform/internal/signals/url_signal_handler.h"

namespace segmentation_platform {

namespace {

// Delay for running clean up task from startup.
const base::TimeDelta kDatabaseCleanupDelayStartup = base::Minutes(2);

// Periodic interval between two cleanup tasks.
const base::TimeDelta kDatabaseCleanupDelayNormal = base::Days(1);

// Number of days to keep UKM metrics in database.
constexpr base::TimeDelta kUkmEntriesTTL = base::Days(30);

}  // namespace

UkmDataManagerImpl::UkmDataManagerImpl() = default;

UkmDataManagerImpl::~UkmDataManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK_EQ(ref_count_, 0);

  if (ukm_observer_) {
    ukm_observer_->set_ukm_data_manager(nullptr);
  }
  url_signal_handler_.reset();
  ukm_database_.reset();
}

void UkmDataManagerImpl::InitializeForTesting(
    std::unique_ptr<UkmDatabase> ukm_database,
    UkmObserver* ukm_observer) {
  InitiailizeImpl(std::move(ukm_database));
  StartObservation(ukm_observer);
}

void UkmDataManagerImpl::Initialize(const base::FilePath& database_path,
                                    bool in_memory) {
  InitiailizeImpl(std::make_unique<UkmDatabaseImpl>(database_path, in_memory));
}

void UkmDataManagerImpl::StartObservation(UkmObserver* ukm_observer) {
  ukm_observer_ = ukm_observer;
  ukm_observer_->set_ukm_data_manager(this);
}

void UkmDataManagerImpl::InitiailizeImpl(
    std::unique_ptr<UkmDatabase> ukm_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK(!ukm_database_);
  DCHECK(!ukm_observer_);

  ukm_database_ = std::move(ukm_database);
  // TODO(ssid): Move this call  to constructor to make it clear any transaction
  // is posted after initialization.
  ukm_database_->InitDatabase(base::DoNothing());

  GetOrCreateUrlHandler();

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UkmDataManagerImpl::RunCleanupTask,
                     weak_factory_.GetWeakPtr()),
      kDatabaseCleanupDelayStartup);
}

bool UkmDataManagerImpl::IsUkmEngineEnabled() {
  // DummyUkmDataManager is created when UKM engine is disabled.
  return true;
}

UrlSignalHandler* UkmDataManagerImpl::GetOrCreateUrlHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK(ukm_database_);
  if (!url_signal_handler_) {
    url_signal_handler_ =
        std::make_unique<UrlSignalHandler>(ukm_database_.get());
  }
  return url_signal_handler_.get();
}

void UkmDataManagerImpl::StartObservingUkm(const UkmConfig& ukm_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  // TODO(b/290821132): Remove this check.
  if (!ukm_observer_) {
    CHECK_IS_TEST();
    return;
  }
  ukm_observer_->StartObserving(ukm_config);
}

void UkmDataManagerImpl::PauseOrResumeObservation(bool pause) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  // TODO(b/290821132): Remove this check.
  if (!ukm_observer_) {
    // On iOS the eg tests do not set this flag.
#if !BUILDFLAG(IS_IOS)
    CHECK_IS_TEST();
#endif
    return;
  }
  ukm_observer_->PauseOrResumeObservation(pause);
}

UkmDatabase* UkmDataManagerImpl::GetUkmDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK(ukm_database_);
  return ukm_database_.get();
}

bool UkmDataManagerImpl::HasUkmDatabase() {
  return ukm_database_ ? true : false;
}

void UkmDataManagerImpl::OnEntryAdded(ukm::mojom::UkmEntryPtr entry) {
  ukm_database_->StoreUkmEntry(std::move(entry));
}

void UkmDataManagerImpl::OnUkmSourceUpdated(ukm::SourceId source_id,
                                            const std::vector<GURL>& urls) {
  if (url_signal_handler_)
    url_signal_handler_->OnUkmSourceUpdated(source_id, urls);
}

void UkmDataManagerImpl::AddRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  ref_count_++;
}

void UkmDataManagerImpl::RemoveRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK_GT(ref_count_, 0);
  ref_count_--;
}

void UkmDataManagerImpl::RunCleanupTask() {
  DCHECK(ukm_database_);
  ukm_database_->DeleteEntriesOlderThan(base::Time::Now() - kUkmEntriesTTL);

  // Consider waiting for the above task to finish successfully before posting
  // the next one.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UkmDataManagerImpl::RunCleanupTask,
                     weak_factory_.GetWeakPtr()),
      kDatabaseCleanupDelayNormal);
}

}  // namespace segmentation_platform
