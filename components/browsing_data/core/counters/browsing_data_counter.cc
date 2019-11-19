// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/browsing_data_counter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"

namespace browsing_data {

namespace {
static const int kDelayUntilShowCalculatingMs = 140;
static const int kDelayUntilReadyToShowResultMs = 1000;
}

BrowsingDataCounter::BrowsingDataCounter()
    : initialized_(false), use_delay_(true), state_(State::IDLE) {}

BrowsingDataCounter::~BrowsingDataCounter() {}

void BrowsingDataCounter::Init(PrefService* pref_service,
                               ClearBrowsingDataTab clear_browsing_data_tab,
                               const Callback& callback) {
  DCHECK(!initialized_);
  callback_ = callback;
  clear_browsing_data_tab_ = clear_browsing_data_tab;
  pref_.Init(GetPrefName(), pref_service,
             base::Bind(&BrowsingDataCounter::Restart, base::Unretained(this)));
  period_.Init(
      GetTimePeriodPreferenceName(GetTab()), pref_service,
      base::Bind(&BrowsingDataCounter::Restart, base::Unretained(this)));

  initialized_ = true;
  OnInitialized();
}

void BrowsingDataCounter::InitWithoutPref(base::Time begin_time,
                                          const Callback& callback) {
  DCHECK(!initialized_);
  use_delay_ = false;
  callback_ = callback;
  clear_browsing_data_tab_ = ClearBrowsingDataTab::ADVANCED;
  begin_time_ = begin_time;
  initialized_ = true;
  OnInitialized();
}

void BrowsingDataCounter::OnInitialized() {}

base::Time BrowsingDataCounter::GetPeriodStart() {
  if (period_.GetPrefName().empty())
    return begin_time_;
  return CalculateBeginDeleteTime(static_cast<TimePeriod>(*period_));
}

base::Time BrowsingDataCounter::GetPeriodEnd() {
  if (period_.GetPrefName().empty())
    return base::Time::Max();
  return CalculateEndDeleteTime(static_cast<TimePeriod>(*period_));
}

void BrowsingDataCounter::Restart() {
  DCHECK(initialized_);
  TRACE_EVENT_ASYNC_BEGIN1("browsing_data", "BrowsingDataCounter::Restart",
                           this, "data_type", GetPrefName());
  if (state_ == State::IDLE) {
    DCHECK(!timer_.IsRunning());
    DCHECK(!staged_result_);
  } else {
    timer_.Stop();
    staged_result_.reset();
  }
  state_ = State::RESTARTED;
  state_transitions_.clear();
  state_transitions_.push_back(state_);

  if (use_delay_) {
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kDelayUntilShowCalculatingMs), this,
        &BrowsingDataCounter::TransitionToShowCalculating);
  } else {
    state_ = State::READY_TO_REPORT_RESULT;
  }
  TRACE_EVENT1("browsing_data", "BrowsingDataCounter::Count", "data_type",
               GetPrefName());
  Count();
}

void BrowsingDataCounter::ReportResult(ResultInt value) {
  ReportResult(std::make_unique<FinishedResult>(this, value));
}

void BrowsingDataCounter::ReportResult(std::unique_ptr<Result> result) {
  DCHECK(initialized_);
  DCHECK(result->Finished());
  TRACE_EVENT_ASYNC_END1("browsing_data", "BrowsingDataCounter::Restart", this,
                         "data_type", GetPrefName());
  switch (state_) {
    case State::RESTARTED:
    case State::READY_TO_REPORT_RESULT:
      DoReportResult(std::move(result));
      return;
    case State::SHOW_CALCULATING:
      staged_result_ = std::move(result);
      return;
    case State::IDLE:
    case State::REPORT_STAGED_RESULT:
      NOTREACHED();
  }
}

void BrowsingDataCounter::DoReportResult(std::unique_ptr<Result> result) {
  DCHECK(initialized_);
  DCHECK(state_ == State::RESTARTED ||
         state_ == State::REPORT_STAGED_RESULT ||
         state_ == State::READY_TO_REPORT_RESULT);
  state_ = State::IDLE;
  state_transitions_.push_back(state_);

  timer_.Stop();
  staged_result_.reset();
  callback_.Run(std::move(result));
}

const std::vector<BrowsingDataCounter::State>&
BrowsingDataCounter::GetStateTransitionsForTesting() {
  return state_transitions_;
}

ClearBrowsingDataTab BrowsingDataCounter::GetTab() const {
  return clear_browsing_data_tab_;
}

void BrowsingDataCounter::TransitionToShowCalculating() {
  DCHECK(initialized_);
  DCHECK_EQ(State::RESTARTED, state_);
  state_ = State::SHOW_CALCULATING;
  state_transitions_.push_back(state_);

  callback_.Run(std::make_unique<Result>(this));
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kDelayUntilReadyToShowResultMs),
      this, &BrowsingDataCounter::TransitionToReadyToReportResult);
}

void BrowsingDataCounter::TransitionToReadyToReportResult() {
  DCHECK(initialized_);
  DCHECK_EQ(State::SHOW_CALCULATING, state_);

  if (staged_result_) {
    state_ = State::REPORT_STAGED_RESULT;
    state_transitions_.push_back(state_);
    DoReportResult(std::move(staged_result_));
  } else {
    state_ = State::READY_TO_REPORT_RESULT;
    state_transitions_.push_back(state_);
  }
}

// BrowsingDataCounter::Result -------------------------------------------------

BrowsingDataCounter::Result::Result(const BrowsingDataCounter* source)
    : source_(source) {
  DCHECK(source);
}

BrowsingDataCounter::Result::~Result() {}

bool BrowsingDataCounter::Result::Finished() const {
  return false;
}

// BrowsingDataCounter::FinishedResult -----------------------------------------

BrowsingDataCounter::FinishedResult::FinishedResult(
    const BrowsingDataCounter* source,
    ResultInt value)
    : Result(source), value_(value) {}

BrowsingDataCounter::FinishedResult::~FinishedResult() {}

bool BrowsingDataCounter::FinishedResult::Finished() const {
  return true;
}

BrowsingDataCounter::ResultInt BrowsingDataCounter::FinishedResult::Value()
    const {
  return value_;
}

// BrowsingDataCounter::SyncResult -----------------------------------------

BrowsingDataCounter::SyncResult::SyncResult(const BrowsingDataCounter* source,
                                            ResultInt value,
                                            bool sync_enabled)
    : FinishedResult(source, value), sync_enabled_(sync_enabled) {}

BrowsingDataCounter::SyncResult::~SyncResult() {}

}  // namespace browsing_data
