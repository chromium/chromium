// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_handler.h"

#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/history_service_observer.h"
#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace segmentation_platform {

SignalHandler::SignalHandler() = default;
SignalHandler::~SignalHandler() = default;

void SignalHandler::Initialize(
    SignalDatabase* signal_database,
    SegmentInfoDatabase* segment_info_database,
    UkmDataManager* ukm_data_manager,
    history::HistoryService* history_service,
    DefaultModelManager* default_model_manager,
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        segment_ids) {
  user_action_signal_handler_ =
      std::make_unique<UserActionSignalHandler>(signal_database);
  histogram_signal_handler_ =
      std::make_unique<HistogramSignalHandler>(signal_database);
  signal_filter_processor_ = std::make_unique<SignalFilterProcessor>(
      segment_info_database, user_action_signal_handler_.get(),
      histogram_signal_handler_.get(), ukm_data_manager, default_model_manager,
      segment_ids);

  if (ukm_data_manager->IsUkmEngineEnabled() && history_service) {
    // If UKM engine is enabled and history service is not available, then we
    // would write metrics without URLs to the database, which is OK.
    history_service_observer_ = std::make_unique<HistoryServiceObserver>(
        history_service, ukm_data_manager->GetOrCreateUrlHandler());
  }
}

void SignalHandler::TearDown() {
  history_service_observer_.reset();
}

void SignalHandler::EnableMetrics(bool signal_collection_allowed) {
  signal_filter_processor_->EnableMetrics(signal_collection_allowed);
}

void SignalHandler::OnSignalListUpdated() {
  signal_filter_processor_->OnSignalListUpdated();
}

}  // namespace segmentation_platform
