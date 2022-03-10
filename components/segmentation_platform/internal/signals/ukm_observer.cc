// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/ukm_observer.h"

#include <cstdint>

#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "components/segmentation_platform/internal/signals/url_signal_handler.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace segmentation_platform {

UkmObserver::UkmObserver(ukm::UkmRecorderImpl* ukm_recorder,
                         UkmDatabase* ukm_database,
                         UrlSignalHandler* url_signal_handler)
    : ukm_database_(ukm_database),
      url_signal_handler_(url_signal_handler),
      ukm_recorder_(ukm_recorder) {}

UkmObserver::~UkmObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  if (config_)
    ukm_recorder_->RemoveUkmRecorderObserver(this);
}

void UkmObserver::StartObserving(const UkmConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);

  bool was_observing = true;
  if (!config_) {
    config_ = std::make_unique<UkmConfig>();
    was_observing = false;
  }

  UkmConfig::MergeResult result = config_->Merge(config);
  if (result == UkmConfig::NO_NEW_EVENT)
    return;

  if (was_observing)
    ukm_recorder_->RemoveUkmRecorderObserver(this);
  ukm_recorder_->AddUkmRecorderObserver(config_->GetRawObservedEvents(), this);
}

void UkmObserver::OnEntryAdded(ukm::mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  if (paused_)
    return;

  // Remove any metric from the entry that is not observed.
  const base::flat_set<UkmMetricHash>* metrics_for_event =
      config_->GetObservedMetrics(
          UkmEventHash::FromUnsafeValue(entry->event_hash));
  if (!metrics_for_event)
    return;
  for (const auto& metric_and_value : entry->metrics) {
    if (!metrics_for_event->count(
            UkmMetricHash::FromUnsafeValue(metric_and_value.first))) {
      entry->metrics.erase(metric_and_value);
    }
  }
  ukm_database_->UkmEntryAdded(std::move(entry));
}

void UkmObserver::OnUpdateSourceURL(ukm::SourceId source_id,
                                    const std::vector<GURL>& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  if (paused_)
    return;

  url_signal_handler_->OnUkmSourceUpdated(source_id, urls);
}

void UkmObserver::PauseOrResumeObservation(bool pause) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  paused_ = pause;
}

}  // namespace segmentation_platform
