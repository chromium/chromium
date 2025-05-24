// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_VOTER_H_
#define COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_VOTER_H_

#include <array>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"

namespace memory_pressure {

// Interface used by code which actually monitors memory pressure, to inform
// a MemoryPressureAggregator when the pressure they observe changes, or they
// want to trigger a (re-)notification of clients of the current level.
// Voters must be used only from the same sequence as the Aggregator to which
// they are attached.
class MemoryPressureVoter {
 public:
  virtual ~MemoryPressureVoter() = default;

  // Called to set a vote / change a vote.
  virtual void SetVote(base::MemoryPressureListener::MemoryPressureLevel level,
                       bool notify_listeners) = 0;
};

// Collects votes from MemoryPressureVoters and evaluates them to determine the
// pressure level for the MultiSourceMemoryPressureMonitor, which will own
// and outlive the aggregator. The pressure level is calculated as the most
// critical of all votes collected. This class is not thread safe and should be
// used from a single sequence.
class MemoryPressureVoteAggregator {
 public:
  class Delegate;

  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

  explicit MemoryPressureVoteAggregator(Delegate* delegate);
  ~MemoryPressureVoteAggregator();

  MemoryPressureVoteAggregator(const MemoryPressureVoteAggregator&) = delete;
  MemoryPressureVoteAggregator& operator=(const MemoryPressureVoteAggregator&) =
      delete;

  // Creates a MemoryPressureVoter attached to this Aggregator. The returned
  // Voter must not out-live the Aggregator.
  std::unique_ptr<MemoryPressureVoter> CreateVoter();

  void OnVoteForTesting(std::optional<MemoryPressureLevel> old_vote,
                        std::optional<MemoryPressureLevel> new_vote);

  void NotifyListenersForTesting();

  base::MemoryPressureListener::MemoryPressureLevel EvaluateVotesForTesting();
  void SetVotesForTesting(size_t none_votes,
                          size_t moderate_votes,
                          size_t critical_votes);

 private:
  friend class MemoryPressureVoterImpl;

  // Invoked by MemoryPressureVoter as it calculates its vote. Optional is
  // used so a voter can pass null as |old_vote| if this is their first vote, or
  // null as |new_vote| if they are removing their vote (e.g. when the voter is
  // being destroyed). |old_vote| and |new_vote| should never both be null.
  void OnVote(std::optional<MemoryPressureLevel> old_vote,
              std::optional<MemoryPressureLevel> new_vote);

  // Triggers a notification of the MemoryPressureMonitor's current pressure
  // level, allowing each of the various sources of input on MemoryPressureLevel
  // to maintain their own signalling behavior.
  // TODO(crbug.com/40639224): Remove this behavior and standardize across
  // platforms.
  void NotifyListeners();

  // Returns the highest index of |votes_| with a non-zero value, as a
  // MemoryPressureLevel.
  MemoryPressureLevel EvaluateVotes() const;

  MemoryPressureLevel current_pressure_level_ =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  const raw_ptr<Delegate> delegate_;

  // Array with one bucket for each potential MemoryPressureLevel. The overall
  // MemoryPressureLevel is calculated as the highest index of a non-zero
  // bucket.
  // MEMORY_PRESSURE_LEVEL_CRITICAL + 1 is used in place of adding a kCount
  // value to the MemoryPressureLevel enum as adding another value would require
  // changing every instance of switch(MemoryPressureLevel) in Chromium, and the
  // MemoryPressureLevel system will be changing soon regardless.
  std::array<size_t,
             base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL + 1>
      votes_ = {};

  SEQUENCE_CHECKER(sequence_checker_);
};

// Interface used to notify MemoryPressureVoteAggregator's owner of changes to
// vote aggregation.
class MemoryPressureVoteAggregator::Delegate {
 public:
  Delegate() = default;
  virtual ~Delegate() = default;

  // Invoked when the aggregate vote has changed.
  virtual void OnMemoryPressureLevelChanged(
      base::MemoryPressureListener::MemoryPressureLevel level) = 0;

  // Invoked when a voter has determined that a notification of the current
  // pressure level is necessary.
  virtual void OnNotifyListenersRequested() = 0;
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_VOTER_H_
