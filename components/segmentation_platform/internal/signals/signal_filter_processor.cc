// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"

#include <set>

#include "base/logging.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/history_service_observer.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform {
namespace {

class FilterExtractor {
 public:
  explicit FilterExtractor(
      const SegmentInfoDatabase::SegmentInfoList& segment_infos) {
    for (auto& info : segment_infos) {
      const proto::SegmentInfo& segment_info = *info.second;
      const auto& metadata = segment_info.model_metadata();
      metadata_utils::VisitAllUmaFeatures(
          metadata, /*include_outputs=*/true,
          base::BindRepeating(&FilterExtractor::AddUmaFeature,
                              base::Unretained(this)));
      if (AddUkmFeatures(metadata)) {
        history_based_segments.insert(segment_info.segment_id());
      }
    }
  }

  std::set<uint64_t> user_actions;
  std::set<std::pair<std::string, proto::SignalType>> histograms;
  UkmConfig ukm_config;
  base::flat_set<SegmentId> history_based_segments;

 private:
  void AddUmaFeature(const proto::UMAFeature& feature) {
    if (feature.type() == proto::SignalType::USER_ACTION &&
        feature.name_hash() != 0) {
      user_actions.insert(feature.name_hash());
      VLOG(1) << "Segmentation platform started observing " << feature.name();
      return;
    }

    if ((feature.type() == proto::SignalType::HISTOGRAM_VALUE ||
         feature.type() == proto::SignalType::HISTOGRAM_ENUM) &&
        !feature.name().empty()) {
      VLOG(1) << "Segmentation platform started observing " << feature.name();
      histograms.insert(std::make_pair(feature.name(), feature.type()));
      return;
    }

    NOTREACHED_IN_MIGRATION() << "Unexpected feature type";
  }

  bool AddUkmFeatures(const proto::SegmentationModelMetadata& metadata) {
    bool has_ukm_features = false;
    for (const auto& feature : metadata.input_features()) {
      for (const auto& ukm_event :
           feature.sql_feature().signal_filter().ukm_events()) {
        base::flat_set<UkmMetricHash> metrics;
        for (const uint64_t metric : ukm_event.metric_hash_filter())
          metrics.insert(UkmMetricHash::FromUnsafeValue(metric));
        ukm_config.AddEvent(
            UkmEventHash::FromUnsafeValue(ukm_event.event_hash()), metrics);
        has_ukm_features = true;
      }
    }
    return has_ukm_features;
  }
};

}  // namespace

SignalFilterProcessor::SignalFilterProcessor(
    StorageService* storage_service,
    UserActionSignalHandler* user_action_signal_handler,
    HistogramSignalHandler* histogram_signal_handler,
    HistoryServiceObserver* history_observer,
    const base::flat_set<SegmentId>& segment_ids)
    : storage_service_(storage_service),
      user_action_signal_handler_(user_action_signal_handler),
      histogram_signal_handler_(histogram_signal_handler),
      history_observer_(history_observer),
      segment_ids_(segment_ids) {}

SignalFilterProcessor::~SignalFilterProcessor() = default;

void SignalFilterProcessor::OnSignalListUpdated() {
  auto available_segments =
      storage_service_->segment_info_database()->GetSegmentInfoForBothModels(
          segment_ids_);
  FilterSignals(std::move(available_segments));
}

void SignalFilterProcessor::FilterSignals(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_infos) {
  FilterExtractor extractor(*segment_infos.get());

  stats::RecordSignalsListeningCount(extractor.user_actions,
                                     extractor.histograms);

  user_action_signal_handler_->SetRelevantUserActions(
      std::move(extractor.user_actions));
  histogram_signal_handler_->SetRelevantHistograms(extractor.histograms);
  storage_service_->ukm_data_manager()->StartObservingUkm(extractor.ukm_config);

  if (history_observer_) {
    history_observer_->SetHistoryBasedSegments(
        std::move(extractor.history_based_segments));
  }
  for (const auto& segment_info : *segment_infos) {
    if (is_first_time_model_update_) {
      stats::RecordModelUpdateTimeDifference(
          segment_info.first, segment_info.second->model_update_time_s());
    }
    storage_service_->signal_storage_config()->OnSignalCollectionStarted(
        segment_info.second->model_metadata());
  }
  is_first_time_model_update_ = false;
}

void SignalFilterProcessor::EnableMetrics(bool enable_metrics) {
  user_action_signal_handler_->EnableMetrics(enable_metrics);
  histogram_signal_handler_->EnableMetrics(enable_metrics);
  storage_service_->ukm_data_manager()->PauseOrResumeObservation(
      /*pause=*/!enable_metrics);
}

}  // namespace segmentation_platform
