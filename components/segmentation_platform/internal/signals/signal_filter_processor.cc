// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"

#include <set>

#include "base/logging.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace segmentation_platform {
namespace {

class FilterExtractor {
 public:
  explicit FilterExtractor(
      const std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>&
          segment_infos) {
    for (const auto& pair : segment_infos) {
      const proto::SegmentInfo& segment_info = pair.second;
      const auto& metadata = segment_info.model_metadata();
      AddUmaFeatures(metadata);
      AddUkmFeatures(metadata);
    }
  }

  std::set<uint64_t> user_actions;
  std::set<std::pair<std::string, proto::SignalType>> histograms;
  UkmConfig ukm_config;

 private:
  void AddUmaFeatures(const proto::SegmentationModelMetadata& metadata) {
    auto features =
        metadata_utils::GetAllUmaFeatures(metadata, /*include_outputs=*/true);
    for (auto const& feature : features) {
      if (feature.type() == proto::SignalType::USER_ACTION &&
          feature.name_hash() != 0) {
        user_actions.insert(feature.name_hash());
        VLOG(1) << "Segmentation platform started observing " << feature.name();
        continue;
      }

      if ((feature.type() == proto::SignalType::HISTOGRAM_VALUE ||
           feature.type() == proto::SignalType::HISTOGRAM_ENUM) &&
          !feature.name().empty()) {
        VLOG(1) << "Segmentation platform started observing " << feature.name();
        histograms.insert(std::make_pair(feature.name(), feature.type()));
        continue;
      }

      NOTREACHED() << "Unexpected feature type";

      // TODO(shaktisahu): We can filter out enum values as an optimization
      // before storing in DB.
    }
  }

  void AddUkmFeatures(const proto::SegmentationModelMetadata& metadata) {
    for (const auto& feature : metadata.input_features()) {
      for (const auto& ukm_event :
           feature.sql_feature().signal_filter().ukm_events()) {
        base::flat_set<UkmMetricHash> metrics;
        for (const uint64_t metric : ukm_event.metric_hash_filter())
          metrics.insert(UkmMetricHash::FromUnsafeValue(metric));
        ukm_config.AddEvent(
            UkmEventHash::FromUnsafeValue(ukm_event.event_hash()), metrics);
      }
    }
  }
};

}  // namespace

SignalFilterProcessor::SignalFilterProcessor(
    SegmentInfoDatabase* segment_database,
    UserActionSignalHandler* user_action_signal_handler,
    HistogramSignalHandler* histogram_signal_handler,
    UkmDataManager* ukm_data_manager)
    : segment_database_(segment_database),
      user_action_signal_handler_(user_action_signal_handler),
      histogram_signal_handler_(histogram_signal_handler),
      ukm_data_manager_(ukm_data_manager) {}

SignalFilterProcessor::~SignalFilterProcessor() = default;

void SignalFilterProcessor::OnSignalListUpdated() {
  segment_database_->GetAllSegmentInfo(base::BindOnce(
      &SignalFilterProcessor::FilterSignals, weak_ptr_factory_.GetWeakPtr()));
}

void SignalFilterProcessor::FilterSignals(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_infos) {
  FilterExtractor extractor(*segment_infos);

  stats::RecordSignalsListeningCount(extractor.user_actions,
                                     extractor.histograms);

  user_action_signal_handler_->SetRelevantUserActions(
      std::move(extractor.user_actions));
  histogram_signal_handler_->SetRelevantHistograms(extractor.histograms);
  ukm_data_manager_->StartObservingUkm(extractor.ukm_config);
}

void SignalFilterProcessor::EnableMetrics(bool enable_metrics) {
  user_action_signal_handler_->EnableMetrics(enable_metrics);
  histogram_signal_handler_->EnableMetrics(enable_metrics);
  ukm_data_manager_->PauseOrResumeObservation(/*pause=*/!enable_metrics);
}

}  // namespace segmentation_platform
