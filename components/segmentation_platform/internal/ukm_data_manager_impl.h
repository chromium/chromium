// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace segmentation_platform {

class UkmDatabase;
class UkmObserver;
class UrlSignalHandler;

class UkmDataManagerImpl : public UkmDataManager {
 public:
  UkmDataManagerImpl();
  ~UkmDataManagerImpl() override;

  UkmDataManagerImpl(const UkmDataManagerImpl&) = delete;
  UkmDataManagerImpl& operator=(const UkmDataManagerImpl&) = delete;

  void InitializeForTesting(std::unique_ptr<UkmDatabase> ukm_database,
                            UkmObserver* ukm_observer);

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

 private:
  // Helper method for initializing this object.
  void InitiailizeImpl(std::unique_ptr<UkmDatabase> ukm_database);

  void RunCleanupTask();

  int ref_count_ = 0;
  raw_ptr<UkmObserver> ukm_observer_ = nullptr;
  std::unique_ptr<UkmDatabase> ukm_database_;
  std::unique_ptr<UrlSignalHandler> url_signal_handler_;
  std::unique_ptr<UkmConfig> pending_ukm_config_;

  std::optional<bool> is_ukm_allowed_;

  SEQUENCE_CHECKER(sequence_check_);

  base::WeakPtrFactory<UkmDataManagerImpl> weak_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_
