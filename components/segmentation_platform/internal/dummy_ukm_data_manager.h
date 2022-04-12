// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_UKM_DATA_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_UKM_DATA_MANAGER_H_

#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace segmentation_platform {

// This class is used by the segmentation services when UKM engine is disabled,
// in the production code.
class DummyUkmDataManager : public UkmDataManager {
 public:
  DummyUkmDataManager();
  ~DummyUkmDataManager() override;

  DummyUkmDataManager(DummyUkmDataManager&) = delete;
  DummyUkmDataManager& operator=(DummyUkmDataManager&) = delete;

  // UkmDataManager implementation:
  void Initialize(const base::FilePath& database_path) override;
  bool IsUkmEngineEnabled() override;
  void NotifyCanObserveUkm(ukm::UkmRecorderImpl* ukm_recorder,
                           PrefService* pref_service) override;
  void StartObservingUkm(const UkmConfig& config) override;
  void PauseOrResumeObservation(bool pause) override;
  void StopObservingUkm() override;
  UrlSignalHandler* GetOrCreateUrlHandler() override;
  UkmDatabase* GetUkmDatabase() override;
  void AddRef() override;
  void RemoveRef() override;
  void OnUkmAllowedStateChanged(bool allowed) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_UKM_DATA_MANAGER_H_
