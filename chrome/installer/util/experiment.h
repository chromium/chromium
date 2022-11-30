// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_EXPERIMENT_H_
#define CHROME_INSTALLER_UTIL_EXPERIMENT_H_

#include "base/time/time.h"
#include "chrome/installer/util/experiment_metrics.h"

namespace installer {

class ExperimentStorage;

// The experiment state for the current user. Experiment state is a combination
// of the per-install ExperimentMetrics and the per-user experiment data.
class Experiment {
 public:
  Experiment();
  Experiment(Experiment&&);
  Experiment(const Experiment&);
  ~Experiment();
  Experiment& operator=(Experiment&&) = default;
  Experiment& operator=(const Experiment&) = default;

  // Initializes this instance based on |metrics|.
  void InitializeFromMetrics(const ExperimentMetrics& metrics);

  // Moves this user into |state|, updating metrics as appropriate.
  void SetState(ExperimentMetrics::State state);

  // Assigns this user to |group|.
  void AssignGroup(int group);

  // Setters for storing data relating to experiment. These should only be
  // called if the experiment is between initial and terminal states.

  // Fill toast location value.
  void SetToastLocation(ExperimentMetrics::ToastLocation location);

  // Fill number of days user was inactive before toast was shown.
  void SetInactiveDays(int days);

  // Fill number of times toast was displayed.
  void SetToastCount(int count);

  // Fill fine grained timestamp for first time the toast was shown.
  void SetDisplayTime(base::Time time);

  // Time delta between user session start and toast display.
  void SetUserSessionUptime(base::TimeDelta time_delta);

  // Time delta between toast display and action taken on toast display.
  void SetActionDelay(base::TimeDelta time_delta);

  const ExperimentMetrics& metrics() const { return metrics_; }

  ExperimentMetrics::State state() const { return state_; }

  int group() const { return group_; }

  ExperimentMetrics::ToastLocation toast_location() const {
    return toast_location_;
  }

  int inactive_days() const { return inactive_days_; }

  int toast_count() const { return toast_count_; }

  base::Time first_display_time() const { return first_display_time_; }

  base::Time latest_display_time() const { return latest_display_time_; }

  base::TimeDelta user_session_uptime() const { return user_session_uptime_; }

  base::TimeDelta action_delay() const { return action_delay_; }

 private:
  friend class ExperimentStorage;

  ExperimentMetrics metrics_ = ExperimentMetrics();
  ExperimentMetrics::State state_ = ExperimentMetrics::kUninitialized;
  int group_ = 0;
  ExperimentMetrics::ToastLocation toast_location_ =
      ExperimentMetrics::kOverTaskbarPin;
  int inactive_days_ = 0;
  int toast_count_ = 0;
  base::Time first_display_time_;
  base::Time latest_display_time_;
  base::TimeDelta user_session_uptime_;
  base::TimeDelta action_delay_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_EXPERIMENT_H_
