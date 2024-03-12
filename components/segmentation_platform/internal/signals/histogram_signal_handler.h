// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTOGRAM_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTOGRAM_SIGNAL_HANDLER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/observer_list.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform {

class SignalDatabase;

// Responsible for listening to histogram sample collection events and
// persisting them to the internal database for future processing.
class HistogramSignalHandler {
 public:
  using RelevantHistograms =
      std::set<std::pair<std::string, proto::SignalType>>;

  class Observer : public base::CheckedObserver {
   public:
    // Called when a histogram signal tracked by segmentation platform is
    // updated and written to database.
    virtual void OnHistogramSignalUpdated(const std::string& histogram_name,
                                          base::HistogramBase::Sample) = 0;
    ~Observer() override = default;

   protected:
    Observer() = default;
  };

  HistogramSignalHandler(const std::string& profie_id,
                         SignalDatabase* signal_database,
                         UkmDatabase* ukm_database);
  virtual ~HistogramSignalHandler();

  // Disallow copy/assign.
  HistogramSignalHandler(const HistogramSignalHandler&) = delete;
  HistogramSignalHandler& operator=(const HistogramSignalHandler&) = delete;

  // Called to notify about a set of histograms which the segmentation models
  // care about.
  virtual void SetRelevantHistograms(const RelevantHistograms& histograms);

  // Called to enable or disable metrics collection for segmentation platform.
  virtual void EnableMetrics(bool enable_metrics);

  // Add/Remove observer for histogram update events.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

 private:
  void OnHistogramSample(proto::SignalType signal_type,
                         const char* histogram_name,
                         uint64_t name_hash,
                         base::HistogramBase::Sample sample);

  void OnSampleWritten(const std::string& histogram_name,
                       base::HistogramBase::Sample sample,
                       bool success);

  const std::string profile_id_;

  // The database storing relevant histogram samples.
  const raw_ptr<SignalDatabase> db_;
  const raw_ptr<UkmDatabase> ukm_db_;

  // Whether or not the segmentation platform should record metrics events.
  bool metrics_enabled_;

  // Tracks the histogram names we are currently listening to along with their
  // corresponding observers.
  using HistogramSignal = std::pair<std::string, proto::SignalType>;
  std::map<
      HistogramSignal,
      std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>>
      histogram_observers_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<HistogramSignalHandler> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTOGRAM_SIGNAL_HANDLER_H_
