// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_METRIC_DATA_COLLECTOR_H_
#define COMPONENTS_REPORTING_METRICS_METRIC_DATA_COLLECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

class MetricRateController;
class MetricReportQueue;
class MetricReportingController;
class ReportingSettings;

// A base class for metric data collection and reporting.
class CollectorBase {
 public:
  CollectorBase(Sampler* sampler, MetricReportQueue* metric_report_queue);

  CollectorBase(const CollectorBase& other) = delete;
  CollectorBase& operator=(const CollectorBase& other) = delete;

  virtual ~CollectorBase();

 protected:
  virtual void Collect();

  virtual void OnMetricDataCollected(MetricData metric_data) = 0;

  virtual void ReportMetricData(
      const MetricData& metric_data,
      base::OnceClosure on_data_reported = base::DoNothing());

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  const raw_ptr<Sampler> sampler_;
  const raw_ptr<MetricReportQueue> metric_report_queue_;

  base::WeakPtrFactory<CollectorBase> weak_ptr_factory_{this};
};

// Class to collect and report metric data only one time when the reporting
// setting is enabled.
class OneShotCollector : public CollectorBase {
 public:
  OneShotCollector(Sampler* sampler,
                   MetricReportQueue* metric_report_queue,
                   ReportingSettings* reporting_settings,
                   const std::string& setting_path,
                   bool setting_enabled_default_value,
                   base::OnceClosure on_data_reported = base::DoNothing());

  OneShotCollector(const OneShotCollector& other) = delete;
  OneShotCollector& operator=(const OneShotCollector& other) = delete;

  ~OneShotCollector() override;

 protected:
  void Collect() override;

  void OnMetricDataCollected(MetricData metric_data) override;

 private:
  std::unique_ptr<MetricReportingController> reporting_controller_;

  base::OnceClosure on_data_reported_;

  bool data_collected_ = false;
};

// Class to collect and report metric data periodically if the reporting setting
// is enabled.
class PeriodicCollector : public CollectorBase {
 public:
  PeriodicCollector(Sampler* sampler,
                    MetricReportQueue* metric_report_queue,
                    ReportingSettings* reporting_settings,
                    const std::string& enable_setting_path,
                    bool setting_enabled_default_value,
                    const std::string& rate_setting_path,
                    base::TimeDelta default_rate,
                    int rate_unit_to_ms = 1);

  PeriodicCollector(const PeriodicCollector& other) = delete;
  PeriodicCollector& operator=(const PeriodicCollector& other) = delete;

  ~PeriodicCollector() override;

 protected:
  void OnMetricDataCollected(MetricData metric_data) override;

 private:
  virtual void StartPeriodicCollection();

  virtual void StopPeriodicCollection();

  // `rate_controller_` should be initialized before `reporting_controller_` as
  // initializing `reporting_controller` will trigger `rate_controller_` call if
  // the setting is enabled.
  const std::unique_ptr<MetricRateController> rate_controller_;
  const std::unique_ptr<MetricReportingController> reporting_controller_;
};

// Interface for events detection in collected metric data.
class EventDetector {
 public:
  virtual ~EventDetector() = default;
  // Check if there is a new event present in `current_metric_data` and return
  // it if found.
  virtual absl::optional<MetricEventType> DetectEvent(
      const MetricData& previous_metric_data,
      const MetricData& current_metric_data) = 0;
};

class AdditionalSamplersCollector {
 public:
  explicit AdditionalSamplersCollector(std::vector<Sampler*> samplers);

  AdditionalSamplersCollector(const AdditionalSamplersCollector& other) =
      delete;
  AdditionalSamplersCollector& operator=(
      const AdditionalSamplersCollector& other) = delete;

  ~AdditionalSamplersCollector();

  void CollectAll(MetricCallback on_all_collected_cb,
                  MetricData metric_data) const;

 private:
  void CollectAdditionalMetricData(uint64_t sampler_index,
                                   MetricCallback on_all_collected_cb,
                                   MetricData metric_data,
                                   MetricData new_metric_data) const;

  const std::vector<Sampler*> samplers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AdditionalSamplersCollector> weak_ptr_factory_{this};
};

// Class to collect metric data periodically, check the collected data for
// events, and report metric and event data if an event is detecetd.
class PeriodicEventCollector : public PeriodicCollector {
 public:
  PeriodicEventCollector(Sampler* sampler,
                         std::unique_ptr<EventDetector> event_detector,
                         std::vector<Sampler*> additional_samplers,
                         MetricReportQueue* metric_report_queue,
                         ReportingSettings* reporting_settings,
                         const std::string& enable_setting_path,
                         bool setting_enabled_default_value,
                         const std::string& rate_setting_path,
                         base::TimeDelta default_rate,
                         int rate_unit_to_ms = 1);

  PeriodicEventCollector(const PeriodicEventCollector& other) = delete;
  PeriodicEventCollector& operator=(const PeriodicEventCollector& other) =
      delete;

  ~PeriodicEventCollector() override;

 protected:
  void OnMetricDataCollected(MetricData metric_data) override;

 private:
  void OnAdditionalMetricDataCollected(MetricData metric_data);

  const std::unique_ptr<EventDetector> event_detector_;

  const std::unique_ptr<AdditionalSamplersCollector>
      additional_samplers_collector_;

  MetricData last_collected_data_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_METRIC_DATA_COLLECTOR_H_
