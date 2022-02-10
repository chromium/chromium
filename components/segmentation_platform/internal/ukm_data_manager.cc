// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/ukm_data_manager.h"

#include "base/check_op.h"

namespace segmentation_platform {

UkmDataManager::UkmDataManager() = default;

UkmDataManager::~UkmDataManager() {
  DCHECK_EQ(ref_count_, 0);
  // TODO(ssid): Destroy signal handler and database here.
}

void UkmDataManager::Initialize(const base::FilePath& database_path) {
  // TODO(ssid): Create database here.
}

UrlSignalHandler* UkmDataManager::GetOrCreateUrlHandler() {
  // TODO(ssid): Return signal handler here.
  return nullptr;
}

void UkmDataManager::CanObserveUkm(ukm::UkmRecorderImpl* ukm_recorder) {
  // TODO(ssid): Create observer here.
}

void UkmDataManager::StopObservingUkm() {
  // TODO(ssid): Destroy observer here.
}

void UkmDataManager::AddRef() {
  ref_count_++;
}

void UkmDataManager::RemoveRef() {
  DCHECK_GT(ref_count_, 0);
  ref_count_--;
}

}  // namespace segmentation_platform
