// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"

#include <set>

#include "base/logging.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/internal/stats.h"

namespace segmentation_platform {

SignalFilterProcessor::SignalFilterProcessor(
    SegmentInfoDatabase* segment_database,
    UserActionSignalHandler* user_action_signal_handler,
    HistogramSignalHandler* histogram_signal_handler)
    : segment_database_(segment_database),
      user_action_signal_handler_(user_action_signal_handler),
      histogram_signal_handler_(histogram_signal_handler) {}

SignalFilterProcessor::~SignalFilterProcessor() = default;

void SignalFilterProcessor::OnSignalListUpdated() {
  segment_database_->GetAllSegmentInfo(base::BindOnce(
      &SignalFilterProcessor::FilterSignals, weak_ptr_factory_.GetWeakPtr()));
}

void SignalFilterProcessor::FilterSignals(
    std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
        segment_infos) {
  std::set<uint64_t> user_actions;
  std::set<std::pair<std::string, proto::SignalType>> histograms;
  for (const auto& pair : segment_infos) {
    const proto::SegmentInfo& segment_info = pair.second;
    const auto& metadata = segment_info.model_metadata();
    for (int i = 0; i < metadata.features_size(); i++) {
      const auto& feature = metadata.features(i);
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

  stats::RecordSignalsListeningCount(user_actions, histograms);

  user_action_signal_handler_->SetRelevantUserActions(std::move(user_actions));
  histogram_signal_handler_->SetRelevantHistograms(histograms);
}

void SignalFilterProcessor::EnableMetrics(bool enable_metrics) {
  user_action_signal_handler_->EnableMetrics(enable_metrics);
  histogram_signal_handler_->EnableMetrics(enable_metrics);
}

}  // namespace segmentation_platform
