// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_voter.h"

#include <numeric>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace memory_pressure {

class MemoryPressureVoterImpl : public MemoryPressureVoter {
 public:
  explicit MemoryPressureVoterImpl(MemoryPressureVoteAggregator* aggregator)
      : aggregator_(aggregator) {}
  ~MemoryPressureVoterImpl() override {
    // Remove this voter's vote.
    if (vote_) {
      aggregator_->OnVote(vote_, std::nullopt);
    }
  }

  MemoryPressureVoterImpl(MemoryPressureVoterImpl&&) = delete;
  MemoryPressureVoterImpl& operator=(MemoryPressureVoterImpl&&) = delete;

  void SetVote(base::MemoryPressureLevel level) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto old_vote = vote_;
    vote_ = {.level = level, .periodic_renotify = true};
    if (aggregator_->OnVote(old_vote, vote_)) {
      aggregator_->NotifyListeners();
    }
  }

  void SetVote(base::MemoryPressureLevel level,
               bool notify_listeners) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto old_vote = vote_;
    vote_ = {.level = level, .periodic_renotify = false};
    aggregator_->OnVote(old_vote, vote_);
    if (notify_listeners) {
      aggregator_->ResetRenotifyTimer();
      aggregator_->NotifyListeners();
    }
  }

 private:
  // This is the aggregator to which this voter's votes will be cast.
  const raw_ptr<MemoryPressureVoteAggregator> aggregator_;

  // optional<> is used here as the vote will be null until the voter's
  // first vote calculation.
  std::optional<MemoryPressureVoteAggregator::Vote> vote_;

  SEQUENCE_CHECKER(sequence_checker_);
};

MemoryPressureVoteAggregator::MemoryPressureVoteAggregator(Delegate* delegate)
    : delegate_(delegate) {}

MemoryPressureVoteAggregator::~MemoryPressureVoteAggregator() {
  DCHECK_EQ(std::accumulate(votes_.begin(), votes_.end(), 0), 0);
  DCHECK_EQ(std::accumulate(periodic_renotify_votes_.begin(),
                            periodic_renotify_votes_.end(), 0),
            0);
}

std::unique_ptr<MemoryPressureVoter>
MemoryPressureVoteAggregator::CreateVoter() {
  return std::make_unique<MemoryPressureVoterImpl>(this);
}

void MemoryPressureVoteAggregator::OnVoteForTesting(
    std::optional<base::MemoryPressureLevel> old_pressure_level,
    std::optional<base::MemoryPressureLevel> new_pressure_level) {
  std::optional<Vote> old_vote = std::nullopt;
  if (old_pressure_level) {
    old_vote = Vote{.level = *old_pressure_level, .periodic_renotify = false};
  }
  std::optional<Vote> new_vote = std::nullopt;
  if (new_pressure_level) {
    new_vote = Vote{.level = *new_pressure_level, .periodic_renotify = false};
  }
  OnVote(old_vote, new_vote);
}

void MemoryPressureVoteAggregator::NotifyListenersForTesting() {
  NotifyListeners();
}

base::MemoryPressureLevel
MemoryPressureVoteAggregator::EvaluateVotesForTesting() {
  return EvaluateVotes();
}

bool MemoryPressureVoteAggregator::OnVote(std::optional<Vote> old_vote,
                                          std::optional<Vote> new_vote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(old_vote || new_vote);
  if (old_vote) {
    DCHECK_LT(0u, votes_[old_vote->level]);
    votes_[old_vote->level]--;
    if (old_vote->periodic_renotify) {
      DCHECK_LT(0u, periodic_renotify_votes_[old_vote->level]);
      periodic_renotify_votes_[old_vote->level]--;
    }
  }
  if (new_vote) {
    votes_[new_vote->level]++;
    if (new_vote->periodic_renotify) {
      periodic_renotify_votes_[new_vote->level]++;
    }
  }
  auto old_pressure_level = current_pressure_level_;

  // If the pressure level is not None then an asynchronous event will have been
  // started below, it needs to be ended.
  // Note that we record this event every time we receive a new vote to ensure
  // that the begin event doesn't get dropped during long pressure sessions.
  if (old_pressure_level == base::MEMORY_PRESSURE_LEVEL_CRITICAL ||
      old_pressure_level == base::MEMORY_PRESSURE_LEVEL_MODERATE) {
    // End MemoryPressure::CriticalPressure/MemoryPressure::ModeratePressure
    // event.
    TRACE_EVENT_END("memory_pressure", perfetto::Track::FromPointer(this));
  }

  current_pressure_level_ = EvaluateVotes();

  // Start an asynchronous tracing event to record this pressure session.
  if (current_pressure_level_ == base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    TRACE_EVENT_BEGIN("memory_pressure", "MemoryPressure::CriticalPressure",
                      perfetto::Track::FromPointer(this));
  } else if (current_pressure_level_ == base::MEMORY_PRESSURE_LEVEL_MODERATE) {
    TRACE_EVENT_BEGIN("memory_pressure", "MemoryPressure::ModeratePressure",
                      perfetto::Track::FromPointer(this));
  }

  const bool level_changed = old_pressure_level != current_pressure_level_;
  if (level_changed) {
    delegate_->OnMemoryPressureLevelChanged(current_pressure_level_);
  }
  // The timer is re-evaluated after every vote, not only when the aggregate
  // level changes: a vote joining or leaving the aggregate level can change
  // whether anyone at that level still wants periodic re-notification.
  UpdateRenotifyTimer(level_changed);
  return level_changed;
}

void MemoryPressureVoteAggregator::UpdateRenotifyTimer(bool level_changed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_pressure_level_ == base::MEMORY_PRESSURE_LEVEL_NONE ||
      periodic_renotify_votes_[current_pressure_level_] == 0) {
    renotify_timer_.Stop();
    return;
  }
  // Leave a running timer alone when the level has not changed, so that
  // additional votes at the current level do not delay the next notification.
  if (renotify_timer_.IsRunning() && !level_changed) {
    return;
  }
  const base::TimeDelta cooldown =
      current_pressure_level_ == base::MEMORY_PRESSURE_LEVEL_CRITICAL
          ? kCriticalPressureCooldown
          : kModeratePressureCooldown;
  renotify_timer_.Start(FROM_HERE, cooldown, this,
                        &MemoryPressureVoteAggregator::NotifyListeners);
}

void MemoryPressureVoteAggregator::ResetRenotifyTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (renotify_timer_.IsRunning()) {
    renotify_timer_.Reset();
  }
}

void MemoryPressureVoteAggregator::NotifyListeners() {
  delegate_->OnNotifyListenersRequested();
}

base::MemoryPressureLevel MemoryPressureVoteAggregator::EvaluateVotes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static_assert(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL == 2,
      "Ensure that each memory pressure level is handled by this method.");
  if (votes_[2]) {
    return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  if (votes_[1]) {
    return base::MEMORY_PRESSURE_LEVEL_MODERATE;
  }
  return base::MEMORY_PRESSURE_LEVEL_NONE;
}

void MemoryPressureVoteAggregator::SetVotesForTesting(size_t none_votes,
                                                      size_t moderate_votes,
                                                      size_t critical_votes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  votes_[0] = none_votes;
  votes_[1] = moderate_votes;
  votes_[2] = critical_votes;
  periodic_renotify_votes_ = {};
}

}  // namespace memory_pressure
