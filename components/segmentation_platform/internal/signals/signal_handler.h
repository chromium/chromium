// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_HANDLER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace history {
class HistoryService;
}

namespace segmentation_platform {

class HistogramSignalHandler;
class HistoryServiceObserver;
class SignalFilterProcessor;
class StorageService;
class UserActionSignalHandler;

// Finds and observes the right signals needed for the models, and stores to the
// database. Only handles signals for profile specific signals. See
// `UkmDataManager` for other UKM related signals.
class SignalHandler {
 public:
  SignalHandler();
  ~SignalHandler();

  SignalHandler(const SignalHandler&) = delete;
  SignalHandler& operator=(const SignalHandler&) = delete;

  void Initialize(StorageService* storage_service,
                  history::HistoryService* history_service,
                  PrefService* profile_prefs,
                  const base::flat_set<proto::SegmentId>& segment_ids,
                  const std::string& profile_id,
                  base::RepeatingClosure model_refresh_callback);

  void TearDown();

  // Called to enable or disable metrics collection for segmentation platform.
  // This is often invoked early even before the signal list is obtained. Must
  // be explicitly called on startup.
  void EnableMetrics(bool signal_collection_allowed);

  // Called whenever the metadata about the models are updated. Registers
  // handlers for the relevant signals specified in the metadata. If handlers
  // are already registered, it will reset and register again with the new set
  // of signals.
  void OnSignalListUpdated();

  // TODO(ssid): This is used for training data observation. Create an observer
  // for this class and remove observer in the internal classes, then remove
  // this method.
  HistogramSignalHandler* deprecated_histogram_signal_handler() {
    return histogram_signal_handler_.get();
  }
  UserActionSignalHandler* user_action_signal_handler() {
    return user_action_signal_handler_.get();
  }

 private:
  std::unique_ptr<UserActionSignalHandler> user_action_signal_handler_;
  std::unique_ptr<HistogramSignalHandler> histogram_signal_handler_;
  // Can be null when UKM engine is disabled.
  std::unique_ptr<HistoryServiceObserver> history_service_observer_;
  std::unique_ptr<SignalFilterProcessor> signal_filter_processor_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_SIGNAL_HANDLER_H_
