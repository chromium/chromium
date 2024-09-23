// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/ukm_observer.h"

#include <cstdint>

#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace segmentation_platform {

UkmObserver::UkmObserver(ukm::UkmRecorderImpl* ukm_recorder)
    : ukm_recorder_(ukm_recorder), ukm_data_manager_(nullptr) {
  // Listen to |OnUkmAllowedStateChanged| event.
  ukm_recorder_->AddUkmRecorderObserver(base::flat_set<uint64_t>(), this);
}

UkmObserver::~UkmObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  StopObserving();
}

void UkmObserver::StartObserving(const UkmConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);

  if (!config_) {
    config_ = std::make_unique<UkmConfig>();
  }

  UkmConfig::MergeResult result = config_->Merge(config);
  if (result == UkmConfig::NO_NEW_EVENT)
    return;

  ukm_recorder_->RemoveUkmRecorderObserver(this);
  ukm_recorder_->AddUkmRecorderObserver(config_->GetRawObservedEvents(), this);
}

void UkmObserver::StopObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  ukm_recorder_->RemoveUkmRecorderObserver(this);
}

void UkmObserver::OnEntryAdded(ukm::mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  if (paused_ || !ukm_data_manager_)
    return;

  // Remove any metric from the entry that is not observed.
  const base::flat_set<UkmMetricHash>* metrics_for_event =
      config_->GetObservedMetrics(
          UkmEventHash::FromUnsafeValue(entry->event_hash));
  if (!metrics_for_event)
    return;

  base::EraseIf(entry->metrics, [&metrics_for_event](const auto& it) {
    return !metrics_for_event->count(UkmMetricHash::FromUnsafeValue(it.first));
  });

  ukm_data_manager_->OnEntryAdded(std::move(entry));
}

void UkmObserver::OnUpdateSourceURL(ukm::SourceId source_id,
                                    const std::vector<GURL>& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  if (paused_ || !ukm_data_manager_)
    return;

  ukm_data_manager_->OnUkmSourceUpdated(source_id, urls);
}

void UkmObserver::PauseOrResumeObservation(bool pause) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  paused_ = pause;
}

void UkmObserver::OnUkmAllowedStateChanged(ukm::UkmConsentState state) {
  InitalizeUkmAllowedState(state.Has(ukm::MSBB));
}

void UkmObserver::InitalizeUkmAllowedState(bool is_msbb_enabled) {
  base::Time most_recent_allowed = LocalStateHelper::GetInstance().GetPrefTime(
      kSegmentationUkmMostRecentAllowedTimeKey);
  if (!is_msbb_enabled) {
    if (most_recent_allowed != base::Time::Max()) {
      LocalStateHelper::GetInstance().SetPrefTime(
          kSegmentationUkmMostRecentAllowedTimeKey, base::Time::Max());
    }
    return;
  }
  // Update the most recent allowed time if needed.
  if (most_recent_allowed.is_null() ||
      most_recent_allowed == base::Time::Max()) {
    LocalStateHelper::GetInstance().SetPrefTime(
        kSegmentationUkmMostRecentAllowedTimeKey, base::Time::Now());
  }
}

}  // namespace segmentation_platform
