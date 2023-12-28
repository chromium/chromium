// Copyright 2022 The Chromium Authors
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

  DummyUkmDataManager(const DummyUkmDataManager&) = delete;
  DummyUkmDataManager& operator=(const DummyUkmDataManager&) = delete;

  // UkmDataManager implementation:
  void Initialize(const base::FilePath& database_path, bool in_memory) override;
  void StartObservation(UkmObserver* ukm_observer) override;
  bool IsUkmEngineEnabled() override;
  void StartObservingUkm(const UkmConfig& config) override;
  void PauseOrResumeObservation(bool pause) override;
  UrlSignalHandler* GetOrCreateUrlHandler() override;
  UkmDatabase* GetUkmDatabase() override;
  bool HasUkmDatabase() override;
  void OnEntryAdded(ukm::mojom::UkmEntryPtr entry) override;
  void OnUkmSourceUpdated(ukm::SourceId source_id,
                          const std::vector<GURL>& urls) override;
  void AddRef() override;
  void RemoveRef() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DUMMY_UKM_DATA_MANAGER_H_
