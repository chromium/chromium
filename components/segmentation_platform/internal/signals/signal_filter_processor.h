// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_FILTER_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_FILTER_PROCESSOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

class HistogramSignalHandler;
class HistoryServiceObserver;
class StorageService;
class UserActionSignalHandler;

// Responsible for listening to the metadata updates for the models and
// registers various signal handlers for the relevant UMA signals specified in
// the metadata.
class SignalFilterProcessor {
 public:
  SignalFilterProcessor(StorageService* storage_service,
                        UserActionSignalHandler* user_action_signal_handler,
                        HistogramSignalHandler* histogram_signal_handler,
                        HistoryServiceObserver* history_observer,
                        const std::vector<OptimizationTarget>& segment_ids);
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
  void FilterSignals(DefaultModelManager::SegmentInfoList segment_infos);

  const raw_ptr<StorageService> storage_service_;
  const raw_ptr<UserActionSignalHandler> user_action_signal_handler_;
  const raw_ptr<HistogramSignalHandler> histogram_signal_handler_;
  const raw_ptr<HistoryServiceObserver> history_observer_;
  std::vector<OptimizationTarget> segment_ids_;

  base::WeakPtrFactory<SignalFilterProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_FILTER_PROCESSOR_H_
