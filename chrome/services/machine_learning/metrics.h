// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_METRICS_H_
#define CHROME_SERVICES_MACHINE_LEARNING_METRICS_H_

#include <string>

#include "base/time/time.h"

namespace machine_learning {
namespace metrics {

// Categorizes status of the ML Service when it is requested.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Should be kept in sync with ChromeMLServiceRequestStatus enum
// in //tools/metrics/histograms/enums.xml.
enum class MLServiceRequestStatus {
  // ML Service is already launched when requested.
  kRequestedServiceLaunched = 0,
  // ML Service is not launched when requested.
  kRequestedServiceNotLaunched = 1,
  kMaxValue = kRequestedServiceNotLaunched
};

// Names of metrics.
extern const char kServiceRequested[];
extern const char kServiceLaunch[];
extern const char kServiceNormalTermination[];
extern const char kServiceCrash[];
extern const char kServiceAliveDuration[];
extern const char kDecisionTreeModelLoadResult[];
extern const char kDecisionTreeModelPredictionResult[];
extern const char kDecisionTreeModelValidationLatency[];
extern const char kDecisionTreeModelEvaluationLatency[];

// Log to UMA histogram when ML Service is requested.
void LogServiceRequested(MLServiceRequestStatus status);
// Log to UMA histogram when ML Service is launched.
void LogServiceLaunch();
// Log to UMA histogram when ML Service terminates normally.
void LogServiceNormalTermination();
// Log to UMA histogram when ML Service crashes.
void LogServiceCrash();
// Log |time| to UMA histogram as the duration when ML Service is alive.
void LogServiceAliveDuration(base::TimeDelta time);

// Helper class to record time elapsed in a scope.
class ScopedLatencyRecorder {
 public:
  // Initializes with given |metric_name| of the UMA histogram and an associated
  // maximum time |time_max|, and initialized the timer saved in |start_time_|.
  explicit ScopedLatencyRecorder(const std::string& metric_name);

  // Saves elapsed time since construction if |DumpRecord()| has not been
  // called.
  ~ScopedLatencyRecorder();

  ScopedLatencyRecorder(const ScopedLatencyRecorder&) = delete;
  ScopedLatencyRecorder& operator=(const ScopedLatencyRecorder&) = delete;

  // Forces logging elapsed time since |start_time_| to the UMA histogram given
  // by |metric_name_| when called the first time.
  void RecordTimeElapsed();

 private:
  bool recorded_ = false;
  const std::string metric_name_;
  base::TimeTicks start_time_;
};

}  // namespace metrics
}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_METRICS_H_
