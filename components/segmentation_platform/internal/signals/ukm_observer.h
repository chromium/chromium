// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_OBSERVER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_OBSERVER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/ukm/ukm_recorder_observer.h"

namespace ukm {
class UkmRecorderImpl;
}

namespace segmentation_platform {

class UkmConfig;
class UkmDataManagerImpl;

// Observes UKM metrics and URL source and calls UKMDataManager on new
// entries or on source URL changes.
class UkmObserver : public ukm::UkmRecorderObserver {
 public:
  explicit UkmObserver(ukm::UkmRecorderImpl* ukm_recorder);
  ~UkmObserver() override;

  UkmObserver(const UkmObserver&) = delete;
  UkmObserver& operator=(const UkmObserver&) = delete;

  // Starts observing with the given |config| if not started. Otherwise, merges
  // the currently observed config with the new |config|, and observes a
  // superset of the metrics.
  void StartObserving(const UkmConfig& config);

  // Pause/Resume observation of UKM. The observer stops sending any signal to
  // database when paused.
  void PauseOrResumeObservation(bool pause);

  // Stop observing |ukm_recorder_|.
  void StopObserving();

  // UkmRecorderObserver implementation:
  void OnEntryAdded(ukm::mojom::UkmEntryPtr entry) override;
  void OnUpdateSourceURL(ukm::SourceId source_id,
                         const std::vector<GURL>& urls) override;
  void OnUkmAllowedStateChanged(ukm::UkmConsentState state) override;

  // Called to initialize UKM state when the observer is created, in case it
  // missed notifications prior to set up. `is_msbb_enabled` should indicate if
  // ukm::MSBB was consented.
  void InitalizeUkmAllowedState(bool is_msbb_enabled);

  void set_ukm_data_manager(UkmDataManagerImpl* ukm_data_manager) {
    ukm_data_manager_ = ukm_data_manager;
  }

  bool is_started_for_testing() const { return !!config_; }
  bool is_paused_for_testing() const { return paused_; }

 private:
  // UkmDataManagerImpl destroys this observer before the UKM service is
  // destroyed.
  raw_ptr<ukm::UkmRecorderImpl, LeakedDanglingUntriaged> const ukm_recorder_;

  // Currently observed config.
  std::unique_ptr<UkmConfig> config_;
  bool paused_ = false;

  // Class that is notified on new entries or on source URL change.
  raw_ptr<UkmDataManagerImpl> ukm_data_manager_;

  SEQUENCE_CHECKER(sequence_check_);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_OBSERVER_H_
