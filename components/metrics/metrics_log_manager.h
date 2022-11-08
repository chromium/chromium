// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOG_MANAGER_H_
#define COMPONENTS_METRICS_METRICS_LOG_MANAGER_H_

#include <stddef.h>

#include <memory>

#include "components/metrics/metrics_log.h"

namespace metrics {

// Manages all the log objects used by a MetricsService implementation.
// TODO(crbug/1052796): Remove this class, and replace uses of this class with
// just a unique_ptr<MetricsLog>.
class MetricsLogManager {
 public:
  MetricsLogManager();

  MetricsLogManager(const MetricsLogManager&) = delete;
  MetricsLogManager& operator=(const MetricsLogManager&) = delete;

  ~MetricsLogManager();

  // Makes |log| the current_log. This should only be called if there is not a
  // current log.
  void BeginLoggingWithLog(std::unique_ptr<MetricsLog> log);

  // Returns the in-progress log.
  MetricsLog* current_log() { return current_log_.get(); }

  // Releases |current_log_| and transfers ownership to the caller.
  std::unique_ptr<MetricsLog> ReleaseCurrentLog();

 private:
  // The log that we are still appending to.
  std::unique_ptr<MetricsLog> current_log_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_MANAGER_H_
