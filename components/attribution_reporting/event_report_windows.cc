// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_report_windows.h"

#include <stddef.h>

#include <iterator>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

constexpr base::TimeDelta kWindowDeadlineOffset = base::Hours(1);

bool EventReportWindowsValid(base::TimeDelta start_time,
                             const base::flat_set<base::TimeDelta>& end_times) {
  return !start_time.is_negative() && !end_times.empty() &&
         *end_times.begin() > start_time;
}

void MaybeTruncate(std::vector<base::TimeDelta>& end_times,
                   base::TimeDelta expiry) {
  DCHECK(base::ranges::is_sorted(end_times));
  while (end_times.size() > 0 && end_times.back() >= expiry) {
    end_times.pop_back();
  }
  end_times.push_back(expiry);
}

base::Time ReportTimeFromDeadline(base::Time source_time,
                                  base::TimeDelta deadline) {
  // Valid conversion reports should always have a valid reporting deadline.
  DCHECK(deadline.is_positive());
  return source_time + deadline + kWindowDeadlineOffset;
}

}  // namespace

absl::optional<EventReportWindows> EventReportWindows::Create(
    base::TimeDelta start_time,
    std::vector<base::TimeDelta> end_times) {
  if (!base::ranges::is_sorted(end_times)) {
    return absl::nullopt;
  }
  base::flat_set<base::TimeDelta> end_times_set(base::sorted_unique,
                                                std::move(end_times));
  if (!EventReportWindowsValid(start_time, end_times_set)) {
    return absl::nullopt;
  }
  return EventReportWindows(start_time, std::move(end_times_set));
}

absl::optional<EventReportWindows> EventReportWindows::CreateAndTruncate(
    base::TimeDelta start_time,
    std::vector<base::TimeDelta> end_times,
    base::TimeDelta expiry) {
  if (expiry <= start_time) {
    return absl::nullopt;
  }
  MaybeTruncate(end_times, expiry);
  return Create(start_time, std::move(end_times));
}

EventReportWindows::EventReportWindows(
    base::TimeDelta start_time,
    base::flat_set<base::TimeDelta> end_times)
    : start_time_(start_time), end_times_(std::move(end_times)) {
  DCHECK(EventReportWindowsValid(start_time_, end_times_));
}

EventReportWindows::EventReportWindows(mojo::DefaultConstruct::Tag) {
  DCHECK(!EventReportWindowsValid(start_time_, end_times_));
}

EventReportWindows::~EventReportWindows() = default;

EventReportWindows::EventReportWindows(const EventReportWindows&) = default;

EventReportWindows& EventReportWindows::operator=(const EventReportWindows&) =
    default;

EventReportWindows::EventReportWindows(EventReportWindows&&) = default;

EventReportWindows& EventReportWindows::operator=(EventReportWindows&&) =
    default;

base::Time EventReportWindows::ComputeReportTime(
    base::Time source_time,
    base::Time trigger_time) const {
  // Follows the steps detailed in
  // https://wicg.github.io/attribution-reporting-api/#obtain-an-event-level-report-delivery-time
  // Starting from step 2.
  DCHECK_LE(source_time, trigger_time);
  base::TimeDelta reporting_window_to_use = *end_times_.rbegin();

  for (base::TimeDelta reporting_window : end_times_) {
    if (source_time + reporting_window < trigger_time) {
      continue;
    }
    reporting_window_to_use = reporting_window;
    break;
  }
  return ReportTimeFromDeadline(source_time, reporting_window_to_use);
}

base::Time EventReportWindows::ReportTimeAtWindow(base::Time source_time,
                                                  int window_index) const {
  DCHECK_GE(window_index, 0);
  DCHECK_LT(static_cast<size_t>(window_index), end_times_.size());

  return ReportTimeFromDeadline(source_time,
                                *std::next(end_times_.begin(), window_index));
}

EventReportWindows::WindowResult EventReportWindows::FallsWithin(
    base::TimeDelta trigger_moment) const {
  DCHECK(!trigger_moment.is_negative());
  if (trigger_moment < start_time_) {
    return WindowResult::kNotStarted;
  }
  if (trigger_moment >= *end_times_.rbegin()) {
    return WindowResult::kPassed;
  }
  return WindowResult::kFallsWithin;
}

base::Value::Dict EventReportWindows::ToJson() const {
  DCHECK(EventReportWindowsValid(start_time_, end_times_));

  base::Value::Dict dict;

  // TODO(tquintanilla): Replace double cast with int.
  dict.Set("start_time", static_cast<double>(start_time_.InSeconds()));

  base::Value::List list;
  for (const auto& end_time : end_times_) {
    // TODO(tquintanilla): Replace double cast with int.
    list.Append(static_cast<double>(end_time.InSeconds()));
  }

  dict.Set("end_times", std::move(list));
  return dict;
}

}  // namespace attribution_reporting
