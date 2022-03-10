// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"

#include "base/check_op.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "components/segmentation_platform/internal/signals/ukm_observer.h"
#include "components/segmentation_platform/internal/signals/url_signal_handler.h"

namespace segmentation_platform {

UkmDataManagerImpl::UkmDataManagerImpl() = default;

UkmDataManagerImpl::~UkmDataManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK_EQ(ref_count_, 0);

  // UKM observer should be destroyed earlier since it uses the database.
  DCHECK(!ukm_observer_);
  url_signal_handler_.reset();
  ukm_database_.reset();
}

void UkmDataManagerImpl::Initialize(const base::FilePath& database_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  ukm_database_ = std::make_unique<UkmDatabase>(database_path);
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

void UkmDataManagerImpl::NotifyCanObserveUkm(
    ukm::UkmRecorderImpl* ukm_recorder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK(ukm_database_);
  ukm_observer_ = std::make_unique<UkmObserver>(
      ukm_recorder, ukm_database_.get(), GetOrCreateUrlHandler());
  if (pending_ukm_config_) {
    ukm_observer_->StartObserving(*pending_ukm_config_);
    pending_ukm_config_.reset();
  }
}

void UkmDataManagerImpl::StartObservingUkm(const UkmConfig& ukm_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  if (ukm_observer_) {
    ukm_observer_->StartObserving(ukm_config);
  } else {
    if (!pending_ukm_config_)
      pending_ukm_config_ = std::make_unique<UkmConfig>();
    pending_ukm_config_->Merge(ukm_config);
  }
}

void UkmDataManagerImpl::PauseOrResumeObservation(bool pause) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  ukm_observer_->PauseOrResumeObservation(pause);
}

void UkmDataManagerImpl::StopObservingUkm() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK(ukm_database_);
  DCHECK(url_signal_handler_);
  ukm_observer_.reset();
}

UkmDatabase* UkmDataManagerImpl::GetUkmDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  DCHECK(ukm_database_);
  return ukm_database_.get();
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

}  // namespace segmentation_platform
