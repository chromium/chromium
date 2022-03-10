// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_OBSERVER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_OBSERVER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/ukm/ukm_recorder_observer.h"

namespace ukm {
class UkmRecorderImpl;
}

namespace segmentation_platform {

class UkmDatabase;
class UrlSignalHandler;
class UkmConfig;

// Observes UKM metrics and URL source and stores the relevant data in UKM
// database.
class UkmObserver : public ukm::UkmRecorderObserver {
 public:
  UkmObserver(ukm::UkmRecorderImpl* ukm_recorder,
              UkmDatabase* ukm_database,
              UrlSignalHandler* url_signal_handler);
  ~UkmObserver() override;

  UkmObserver(UkmObserver&) = delete;
  UkmObserver& operator=(UkmObserver&) = delete;

  // Starts observing with the given |config| if not started. Otherwise, merges
  // the currently observed config with the new |config|, and observes a
  // superset of the metrics.
  void StartObserving(const UkmConfig& config);

  // Pause/Resume observation of UKM. The observer stops sending any signal to
  // database when paused.
  void PauseOrResumeObservation(bool pause);

  // UkmRecorderObserver implementation:
  void OnEntryAdded(ukm::mojom::UkmEntryPtr entry) override;
  void OnUpdateSourceURL(ukm::SourceId source_id,
                         const std::vector<GURL>& urls) override;

 private:
  // Owned by UkmDataManagerImpl. The manager guarantees that database is
  // deleted after observer.
  raw_ptr<UkmDatabase> ukm_database_;
  // Owned by UkmDataManagerImpl. The manager guarantees that handler is deleted
  // after observer.
  raw_ptr<UrlSignalHandler> url_signal_handler_;
  // UkmDataManagerImpl destroys this observer before the UKM service is
  // destroyed.
  raw_ptr<ukm::UkmRecorderImpl> const ukm_recorder_;

  // Currently observed config.
  std::unique_ptr<UkmConfig> config_;
  bool paused_ = false;

  SEQUENCE_CHECKER(sequence_check_);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_OBSERVER_H_
