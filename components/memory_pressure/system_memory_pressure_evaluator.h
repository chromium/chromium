// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/time/time.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

namespace memory_pressure {

// Base class for the platform SystemMemoryPressureEvaluators, which use
// MemoryPressureVoters to cast their vote on the overall MemoryPressureLevel.
class SystemMemoryPressureEvaluator {
 public:
  // The period at which the system is re-notified when the pressure is not
  // none.
  static const base::TimeDelta kRenotifyVotePeriod;

  // Used by the MemoryPressureMonitor to create the correct Evaluator for the
  // platform in use.
  static std::unique_ptr<SystemMemoryPressureEvaluator>
  CreateDefaultSystemEvaluator(MultiSourceMemoryPressureMonitor* monitor);

  virtual ~SystemMemoryPressureEvaluator();

  SystemMemoryPressureEvaluator(const SystemMemoryPressureEvaluator&) = delete;
  SystemMemoryPressureEvaluator& operator=(
      const SystemMemoryPressureEvaluator&) = delete;

  base::MemoryPressureListener::MemoryPressureLevel current_vote() const {
    return current_vote_;
  }

 protected:
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);

  // Sets the Evaluator's |current_vote_| member without casting vote to the
  // MemoryPressureVoteAggregator.
  void SetCurrentVote(base::MemoryPressureListener::MemoryPressureLevel level);

  // Uses the Evaluators' |voter_| to cast/update its vote on memory pressure
  // level. The MemoryPressureListeners will only be notified of the newly
  // calculated pressure level if |notify| is true.
  void SendCurrentVote(bool notify) const;

 private:
  base::MemoryPressureListener::MemoryPressureLevel current_vote_;

  // In charge of forwarding votes from here to the
  // MemoryPressureVoteAggregator.
  std::unique_ptr<MemoryPressureVoter> voter_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
