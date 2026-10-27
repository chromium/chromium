// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_VOTER_H_
#define COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_VOTER_H_

#include <array>
#include <optional>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"

namespace memory_pressure {

// Interface used by code which actually monitors memory pressure, to inform
// a MemoryPressureAggregator when the pressure they observe changes, or they
// want to trigger a (re-)notification of clients of the current level.
// Voters must be used only from the same sequence as the Aggregator to which
// they are attached.
class MemoryPressureVoter {
 public:
  virtual ~MemoryPressureVoter() = default;

  // Called to set a vote / change a vote. The aggregator will notify
  // listeners when the aggregate pressure level changes, and periodically
  // re-notify them at the cooldown interval while this vote matches the
  // aggregate pressure level.
  virtual void SetVote(base::MemoryPressureLevel level) = 0;

  // Called to set a vote / change a vote when the caller manages its own
  // notification cadence. Listeners are notified if and only if
  // |notify_listeners| is true. The aggregator does not re-notify listeners
  // periodically on behalf of this vote: if it is the only vote at the
  // aggregate pressure level, the re-notification timer is stopped.
  virtual void SetVote(base::MemoryPressureLevel level,
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

  // The cooldown period between repeated notifications at a given memory
  // pressure level.
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  static constexpr base::TimeDelta kModeratePressureCooldown = base::Seconds(5);
#else
  static constexpr base::TimeDelta kModeratePressureCooldown =
      base::Seconds(10);
#endif
  static constexpr base::TimeDelta kCriticalPressureCooldown = base::Seconds(5);

  explicit MemoryPressureVoteAggregator(Delegate* delegate);
  ~MemoryPressureVoteAggregator();

  MemoryPressureVoteAggregator(const MemoryPressureVoteAggregator&) = delete;
  MemoryPressureVoteAggregator& operator=(const MemoryPressureVoteAggregator&) =
      delete;

  // Creates a MemoryPressureVoter attached to this Aggregator. The returned
  // Voter must not out-live the Aggregator. |is_disk_voter| identifies voters
  // whose critical votes represent low disk space rather than RAM pressure.
  std::unique_ptr<MemoryPressureVoter> CreateVoter(bool is_disk_voter = false);

  void OnVoteForTesting(
      std::optional<base::MemoryPressureLevel> old_pressure_level,
      std::optional<base::MemoryPressureLevel> new_pressure_level,
      bool is_disk_voter = false);

  void NotifyListenersForTesting();

  base::MemoryPressureLevel EvaluateVotesForTesting();
  void SetVotesForTesting(size_t none_votes,
                          size_t moderate_votes,
                          size_t critical_votes);

 private:
  friend class MemoryPressureVoterImpl;

  // A vote cast by a MemoryPressureVoter.
  struct Vote {
    base::MemoryPressureLevel level;
    // True if the aggregator should periodically re-notify listeners while
    // |level| is the aggregate pressure level (i.e. the vote was cast through
    // the single-argument SetVote()).
    bool periodic_renotify;
  };

  // Invoked by MemoryPressureVoter as it calculates its vote. Optional is
  // used so a voter can pass null as |old_vote| if this is their first vote, or
  // null as |new_vote| if they are removing their vote (e.g. when the voter is
  // being destroyed). |old_vote| and |new_vote| should never both be null.
  // Returns true if the aggregate pressure level changed as a result of this
  // vote.
  bool OnVote(std::optional<Vote> old_vote,
              std::optional<Vote> new_vote,
              bool is_disk_voter = false);

  // Returns true if the disk voter is at CRITICAL and no other voter is at
  // CRITICAL.
  bool IsDiskOnlyCritical() const;

  // Starts or stops |renotify_timer_| according to |current_pressure_level_|
  // and the votes cast at that level. The timer runs only while at least one
  // vote with |periodic_renotify| set matches |current_pressure_level_|.
  // |level_changed| indicates whether this update follows a change of
  // |current_pressure_level_|, in which case a running timer is restarted
  // with the cooldown of the new level.
  void UpdateRenotifyTimer(bool level_changed);

  // Resets |renotify_timer_| if it is currently running.
  void ResetRenotifyTimer();

  // Triggers a notification of the MemoryPressureMonitor's current pressure
  // level.
  void NotifyListeners();

  // Returns the highest index of |votes_| with a non-zero value, as a
  // MemoryPressureLevel.
  base::MemoryPressureLevel EvaluateVotes() const;

  base::MemoryPressureLevel current_pressure_level_ =
      base::MEMORY_PRESSURE_LEVEL_NONE;

  const raw_ptr<Delegate> delegate_;

  // Array with one bucket for each potential MemoryPressureLevel. The overall
  // MemoryPressureLevel is calculated as the highest index of a non-zero
  // bucket.
  // MEMORY_PRESSURE_LEVEL_CRITICAL + 1 is used in place of adding a kCount
  // value to the MemoryPressureLevel enum as adding another value would require
  // changing every instance of switch(MemoryPressureLevel) in Chromium, and the
  // MemoryPressureLevel system will be changing soon regardless.
  std::array<size_t, base::MEMORY_PRESSURE_LEVEL_CRITICAL + 1> votes_ = {};

  // Subset of |votes_| that were cast with |Vote::periodic_renotify| set.
  std::array<size_t, base::MEMORY_PRESSURE_LEVEL_CRITICAL + 1>
      periodic_renotify_votes_ = {};

  // At most one disk voter may exist at a time.
  enum class DiskVoterState { kNoDiskVoter, kNotCritical, kCritical };
  DiskVoterState disk_voter_state_ = DiskVoterState::kNoDiskVoter;

  // Timer that re-notifies listeners of the current pressure level at regular
  // intervals while pressure is MODERATE or CRITICAL and at least one vote at
  // that level requested periodic re-notification.
  base::RepeatingTimer renotify_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Interface used to notify MemoryPressureVoteAggregator's owner of changes to
// vote aggregation.
class MemoryPressureVoteAggregator::Delegate {
 public:
  Delegate() = default;
  virtual ~Delegate() = default;

  // Invoked when the aggregate vote has changed. |is_disk_only_critical| is
  // true if |level| is CRITICAL solely because of the disk voter.
  virtual void OnMemoryPressureLevelChanged(base::MemoryPressureLevel level,
                                            bool is_disk_only_critical) = 0;

  // Invoked when the aggregate level stays CRITICAL, but switches between
  // being caused solely by the disk voter and not.
  virtual void OnDiskOnlyCriticalChanged(bool is_disk_only_critical) {}

  // Invoked when listeners should be notified of the current pressure level.
  virtual void OnNotifyListenersRequested() = 0;
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_VOTER_H_
