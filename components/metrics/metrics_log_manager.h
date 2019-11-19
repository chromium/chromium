// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOG_MANAGER_H_
#define COMPONENTS_METRICS_METRICS_LOG_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/metrics/metrics_log.h"

namespace metrics {

class MetricsLogStore;

// Manages all the log objects used by a MetricsService implementation. Keeps
// track of an in-progress log and a paused log.
class MetricsLogManager {
 public:
  MetricsLogManager();
  ~MetricsLogManager();

  // Makes |log| the current_log. This should only be called if there is not a
  // current log.
  void BeginLoggingWithLog(std::unique_ptr<MetricsLog> log);

  // Returns the in-progress log.
  MetricsLog* current_log() { return current_log_.get(); }

  // Closes |current_log_|, compresses it, and stores it in the |log_store| for
  // later, leaving |current_log_| nullptr.
  void FinishCurrentLog(MetricsLogStore* log_store);

  // Closes and discards |current_log|.
  void DiscardCurrentLog();

  // Sets current_log to nullptr, but saves the current log for future use with
  // ResumePausedLog(). Only one log may be paused at a time.
  // TODO(stuartmorgan): Pause/resume support is really a workaround for a
  // design issue in initial log writing; that should be fixed, and pause/resume
  // removed.
  void PauseCurrentLog();

  // Restores the previously paused log (if any) to current_log().
  // This should only be called if there is not a current log.
  void ResumePausedLog();

 private:
  // The log that we are still appending to.
  std::unique_ptr<MetricsLog> current_log_;

  // A paused, previously-current log.
  std::unique_ptr<MetricsLog> paused_log_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogManager);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_MANAGER_H_
