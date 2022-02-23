// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_FILTER_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_FILTER_PROCESSOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

class HistogramSignalHandler;
class UserActionSignalHandler;
class UkmDataManager;

// Responsible for listening to the metadata updates for the models and
// registers various signal handlers for the relevant UMA signals specified in
// the metadata.
class SignalFilterProcessor {
 public:
  SignalFilterProcessor(SegmentInfoDatabase* segment_database,
                        UserActionSignalHandler* user_action_signal_handler,
                        HistogramSignalHandler* histogram_signal_handler,
                        UkmDataManager* ukm_data_manager);
  ~SignalFilterProcessor();

  // Disallow copy/assign.
  SignalFilterProcessor(const SignalFilterProcessor&) = delete;
  SignalFilterProcessor& operator=(const SignalFilterProcessor&) = delete;

  // Called whenever the metadata about the models are updated. Registers
  // handlers for the relevant signals specified in the metadata. If handlers
  // are already registered, it will reset and register again with the new set
  // of signals.
  void OnSignalListUpdated();

  // Called to enable or disable metrics collection for segmentation platform.
  // This is often invoked early even before the signal list is obtained. Must
  // be explicitly called on startup.
  void EnableMetrics(bool enable_metrics);

 private:
  void FilterSignals(
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_infos);

  raw_ptr<SegmentInfoDatabase> segment_database_;
  raw_ptr<UserActionSignalHandler> user_action_signal_handler_;
  raw_ptr<HistogramSignalHandler> histogram_signal_handler_;
  raw_ptr<UkmDataManager> ukm_data_manager_;

  base::WeakPtrFactory<SignalFilterProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_FILTER_PROCESSOR_H_
