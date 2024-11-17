// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"

namespace ash::boca {

// Session manager implementation that is primarily used for recording metrics
// throughout a Boca session.
class BocaMetricsManager : public boca::BocaSessionManager::Observer {
 public:
  BocaMetricsManager(bool is_producer);
  BocaMetricsManager(const BocaMetricsManager&) = delete;
  BocaMetricsManager& operator=(const BocaMetricsManager&) = delete;
  ~BocaMetricsManager() override;

  // BocaSessionManager::Observer:
  void OnSessionStarted(const std::string& session_id,
                        const ::boca::UserIdentity& producer) override;
  void OnSessionEnded(const std::string& session_id) override;
  void OnBundleUpdated(const ::boca::Bundle& bundle) override;
  void OnSessionRosterUpdated(const ::boca::Roster& roster) override;

 private:
  // Calculates the duration for the specified `locked_state`. `locked_state` is
  // true when the bundle before the switch was in locked mode, false if it was
  // unlocked.
  void CalculateDurationForContentState(bool locked_state);

  // Determines if this manager is responsible for gathering metrics for a
  // producer or a consumer profile.
  bool is_producer_ = false;

  // Time when the session last switched to a different locked mode.
  base::TimeTicks last_switch_locked_mode_timestamp_;
  base::TimeDelta locked_mode_cumulative_duration_;
  base::TimeDelta unlocked_mode_cumulative_duration_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Set of emails of students that joined the session via code.
  base::flat_set<std::string> students_join_via_code_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Current number of tabs in the bundle sent by the provider.
  int num_of_tabs_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Max number of tabs in the bundle sent by the provider during a session.
  int max_num_of_tabs_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Keeps track of the previous locked state before the switch happens.
  bool is_lock_window_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_MANAGER_H_
