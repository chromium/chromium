// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_

#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace segmentation_platform {

class UkmDataManagerImpl : public UkmDataManager {
 public:
  UkmDataManagerImpl();
  ~UkmDataManagerImpl() override;

  UkmDataManagerImpl(UkmDataManagerImpl&) = delete;
  UkmDataManagerImpl& operator=(UkmDataManagerImpl&) = delete;

  // UkmDataManager implementation:
  void Initialize(const base::FilePath& database_path) override;
  bool IsUkmEngineEnabled() override;
  void NotifyCanObserveUkm(ukm::UkmRecorderImpl* ukm_recorder) override;
  void StartObservingUkm(const UkmConfig& config) override;
  void PauseOrResumeObservation(bool pause) override;
  void StopObservingUkm() override;
  UrlSignalHandler* GetOrCreateUrlHandler() override;
  UkmDatabase* GetUkmDatabase() override;
  void AddRef() override;
  void RemoveRef() override;

 private:
  int ref_count_ = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_
