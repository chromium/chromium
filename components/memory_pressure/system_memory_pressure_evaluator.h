// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

namespace memory_pressure {

// Base class for the platform SystemMemoryPressureEvaluators, which use
// MemoryPressureVoters to cast their vote on the overall MemoryPressureLevel.
class SystemMemoryPressureEvaluator {
 public:
  // Used by the MemoryPressureMonitor to create the correct Evaluator for the
  // platform in use.
  static std::unique_ptr<SystemMemoryPressureEvaluator>
  CreateDefaultSystemEvaluator(MultiSourceMemoryPressureMonitor* monitor);

  virtual ~SystemMemoryPressureEvaluator();

  SystemMemoryPressureEvaluator(const SystemMemoryPressureEvaluator&) = delete;
  SystemMemoryPressureEvaluator& operator=(
      const SystemMemoryPressureEvaluator&) = delete;

  base::MemoryPressureLevel current_vote() const { return current_vote_; }

 protected:
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);

  // Sets the Evaluator's |current_vote_| member without casting vote to the
  // MemoryPressureVoteAggregator.
  void SetCurrentVote(base::MemoryPressureLevel level);

  // Uses the Evaluator's |voter_| to cast/update its vote on memory pressure
  // level. MemoryPressureListeners will be notified of the newly calculated
  // pressure level if the aggregate level changes, and periodically
  // re-notified at the cooldown interval while under pressure.
  void SendCurrentVote() const;

  // Uses the Evaluator's |voter_| to cast/update its vote on memory pressure
  // level when the evaluator manages its own notification cadence.
  // MemoryPressureListeners will be notified of the newly calculated pressure
  // level if and only if |notify| is true.
  void SendCurrentVote(bool notify) const;

 private:
  base::MemoryPressureLevel current_vote_;

  // In charge of forwarding votes from here to the
  // MemoryPressureVoteAggregator.
  std::unique_ptr<MemoryPressureVoter> voter_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
