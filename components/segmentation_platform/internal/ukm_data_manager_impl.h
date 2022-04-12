// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace segmentation_platform {

class UkmDatabase;
class UkmObserver;
class UrlSignalHandler;

class UkmDataManagerImpl : public UkmDataManager {
 public:
  UkmDataManagerImpl();
  ~UkmDataManagerImpl() override;

  UkmDataManagerImpl(UkmDataManagerImpl&) = delete;
  UkmDataManagerImpl& operator=(UkmDataManagerImpl&) = delete;

  void InitializeForTesting(std::unique_ptr<UkmDatabase> ukm_database);

  // Gets the most recent time when UKM is allowed.
  base::Time GetUkmMostRecentAllowedTime() const;

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

 private:
  int ref_count_ = 0;
  std::unique_ptr<UkmDatabase> ukm_database_;
  std::unique_ptr<UrlSignalHandler> url_signal_handler_;
  std::unique_ptr<UkmObserver> ukm_observer_;
  std::unique_ptr<UkmConfig> pending_ukm_config_;

  absl::optional<bool> is_ukm_allowed_;

  raw_ptr<PrefService> prefs_ = nullptr;
  SEQUENCE_CHECKER(sequence_check_);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_IMPL_H_
