// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"

#include "base/check_op.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"

namespace segmentation_platform {

UkmDataManagerImpl::UkmDataManagerImpl() = default;

UkmDataManagerImpl::~UkmDataManagerImpl() {
  DCHECK_EQ(ref_count_, 0);
  // TODO(ssid): Destroy signal handler and database here.
}

void UkmDataManagerImpl::Initialize(const base::FilePath& database_path) {
  // TODO(ssid): Create database here.
}

bool UkmDataManagerImpl::IsUkmEngineEnabled() {
  // DummyUkmDataManager is created when UKM engine is disabled.
  return true;
}

UrlSignalHandler* UkmDataManagerImpl::GetOrCreateUrlHandler() {
  // TODO(ssid): Return signal handler here.
  return nullptr;
}

void UkmDataManagerImpl::NotifyCanObserveUkm(
    ukm::UkmRecorderImpl* ukm_recorder) {
  // TODO(ssid): Create observer here.
}

void UkmDataManagerImpl::StartObservingUkm(const UkmConfig& ukm_config) {
  // TODO(ssid): Implement this.
}

void UkmDataManagerImpl::PauseOrResumeObservation(bool pause) {
  // TODO(ssid): Implement this.
}

void UkmDataManagerImpl::StopObservingUkm() {
  // TODO(ssid): Destroy observer here.
}

UkmDatabase* UkmDataManagerImpl::GetUkmDatabase() {
  // TODO(ssid): Implement.
  return nullptr;
}

void UkmDataManagerImpl::AddRef() {
  ref_count_++;
}

void UkmDataManagerImpl::RemoveRef() {
  DCHECK_GT(ref_count_, 0);
  ref_count_--;
}

}  // namespace segmentation_platform
