// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"

#include <set>

#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"

namespace segmentation_platform {

SignalFilterProcessor::SignalFilterProcessor(
    SegmentInfoDatabase* segment_database,
    UserActionSignalHandler* user_action_signal_handler)
    : segment_database_(segment_database),
      user_action_signal_handler_(user_action_signal_handler) {}

SignalFilterProcessor::~SignalFilterProcessor() = default;

void SignalFilterProcessor::OnSignalListUpdated() {
  segment_database_->GetAllSegmentInfo(base::BindOnce(
      &SignalFilterProcessor::FilterSignals, weak_ptr_factory_.GetWeakPtr()));
}

void SignalFilterProcessor::FilterSignals(
    std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
        segment_infos) {
  std::set<uint64_t> user_actions;
  for (const auto& pair : segment_infos) {
    const proto::SegmentInfo& segment_info = pair.second;
    const auto& metadata = segment_info.model_metadata();
    for (int i = 0; i < metadata.features_size(); i++) {
      const auto& feature = metadata.features(i);
      if (feature.has_user_action() &&
          feature.user_action().has_user_action_hash()) {
        user_actions.insert(feature.user_action().user_action_hash());
      }
      // TODO(shaktisahu): Do the same for enum and value histograms.
    }
  }

  user_action_signal_handler_->SetRelevantUserActions(user_actions);
}

void SignalFilterProcessor::EnableMetrics(bool enable_metrics) {
  user_action_signal_handler_->EnableMetrics(enable_metrics);
}

}  // namespace segmentation_platform
