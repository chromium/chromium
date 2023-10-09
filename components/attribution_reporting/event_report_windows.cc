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
#include "base/functional/not_fn.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

constexpr char kEventReportWindow[] = "event_report_window";
constexpr char kEventReportWindows[] = "event_report_windows";
constexpr char kStartTime[] = "start_time";
constexpr char kEndTimes[] = "end_times";

bool EventReportWindowsValid(base::TimeDelta start_time,
                             const base::flat_set<base::TimeDelta>& end_times) {
  // TODO(apaseltiner): This should also check `*end_times.begin() >=
  // kMinReportWindow` but essentially all unit tests in
  // //content/browser/attribution_reporting would need to be updated as a
  // result based on their use of sub-`kMinReportWindow` expiries.
  return !start_time.is_negative() && !end_times.empty() &&
         end_times.size() <= kMaxEventLevelReportWindows &&
         *end_times.begin() > start_time;
}

bool EventReportWindowValid(base::TimeDelta window) {
  return !window.is_negative();
}

bool IsStrictlyIncreasing(const std::vector<base::TimeDelta>& end_times) {
  return base::ranges::adjacent_find(end_times, base::not_fn(std::less{})) ==
         end_times.end();
}

// TODO(tquintanilla): Consolidate with `MaybeTruncate()`.
void AppendAndMaybeTruncate(std::vector<base::TimeDelta>& end_times,
                            base::TimeDelta expiry) {
  DCHECK(IsStrictlyIncreasing(end_times));
  while (end_times.size() > 0 && end_times.back() >= expiry) {
    end_times.pop_back();
  }
  end_times.push_back(expiry);
}

base::Time ReportTimeFromDeadline(base::Time source_time,
                                  base::TimeDelta deadline) {
  // Valid conversion reports should always have a valid reporting deadline.
  DCHECK(deadline.is_positive());
  return source_time + deadline;
}

}  // namespace

// static
absl::optional<EventReportWindows> EventReportWindows::CreateSingularWindow(
    base::TimeDelta report_window) {
  if (!EventReportWindowValid(report_window)) {
    return absl::nullopt;
  }
  return EventReportWindows(report_window);
}

// static
absl::optional<EventReportWindows> EventReportWindows::CreateWindows(
    base::TimeDelta start_time,
    std::vector<base::TimeDelta> end_times) {
  if (!IsStrictlyIncreasing(end_times)) {
    return absl::nullopt;
  }
  base::flat_set<base::TimeDelta> end_times_set(base::sorted_unique,
                                                std::move(end_times));
  if (!EventReportWindowsValid(start_time, end_times_set)) {
    return absl::nullopt;
  }
  return EventReportWindows(start_time, std::move(end_times_set));
}

// static
absl::optional<EventReportWindows> EventReportWindows::CreateWindowsAndTruncate(
    base::TimeDelta start_time,
    std::vector<base::TimeDelta> end_times,
    base::TimeDelta expiry) {
  if (expiry <= start_time) {
    return absl::nullopt;
  }
  AppendAndMaybeTruncate(end_times, expiry);
  return CreateWindows(start_time, std::move(end_times));
}

base::TimeDelta EventReportWindows::start_time() const {
  DCHECK(!OnlySingularWindow());
  return start_time_or_window_time_;
}

base::TimeDelta EventReportWindows::window_time() const {
  DCHECK(OnlySingularWindow());
  return start_time_or_window_time_;
}

EventReportWindows::EventReportWindows(
    base::TimeDelta start_time,
    base::flat_set<base::TimeDelta> end_times)
    : start_time_or_window_time_(start_time), end_times_(std::move(end_times)) {
  DCHECK(EventReportWindowsValid(start_time_or_window_time_, end_times_));
}

EventReportWindows::EventReportWindows(base::TimeDelta window_time)
    : start_time_or_window_time_(window_time), end_times_({}) {
  DCHECK(EventReportWindowValid(start_time_or_window_time_));
}

EventReportWindows::EventReportWindows() = default;

EventReportWindows::~EventReportWindows() = default;

EventReportWindows::EventReportWindows(const EventReportWindows&) = default;

EventReportWindows& EventReportWindows::operator=(const EventReportWindows&) =
    default;

EventReportWindows::EventReportWindows(EventReportWindows&&) = default;

EventReportWindows& EventReportWindows::operator=(EventReportWindows&&) =
    default;

bool EventReportWindows::MaybeTruncate(base::TimeDelta report_window) {
  if (report_window <= start_time_or_window_time_) {
    return false;
  }
  if (*end_times_.rbegin() >= report_window) {
    while (end_times_.size() > 0 && *end_times_.rbegin() >= report_window) {
      end_times_.erase(std::prev(end_times_.end()));
    }
    end_times_.insert(report_window);
  }
  return true;
}

base::Time EventReportWindows::ComputeReportTime(
    base::Time source_time,
    base::Time trigger_time) const {
  // Follows the steps detailed in
  // https://wicg.github.io/attribution-reporting-api/#obtain-an-event-level-report-delivery-time
  // Starting from step 2.
  DCHECK_LE(source_time, trigger_time);
  base::TimeDelta reporting_window_to_use = *end_times_.rbegin();

  for (base::TimeDelta reporting_window : end_times_) {
    if (source_time + reporting_window <= trigger_time) {
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
  // It is possible for a source to have an assigned time of T and a trigger
  // that is attributed to it to have a time of T-X e.g. due to user-initiated
  // clock changes.
  //
  // TODO(crbug.com/1489333): Assume trigger moment is not negative once
  // attribution time resolution is implemented in storage.
  base::TimeDelta bounded_trigger_moment =
      trigger_moment.is_negative() ? base::Microseconds(0) : trigger_moment;

  if (bounded_trigger_moment < start_time_or_window_time_) {
    return WindowResult::kNotStarted;
  }
  if (bounded_trigger_moment >= *end_times_.rbegin()) {
    return WindowResult::kPassed;
  }
  return WindowResult::kFallsWithin;
}

base::expected<absl::optional<EventReportWindows>, SourceRegistrationError>
EventReportWindows::FromJSON(const base::Value::Dict& registration) {
  const base::Value* singular_window = registration.Find(kEventReportWindow);
  const base::Value* multiple_windows = registration.Find(kEventReportWindows);
  if (singular_window && multiple_windows) {
    return base::unexpected(
        SourceRegistrationError::kBothEventReportWindowFieldsFound);
  }

  if (singular_window) {
    base::TimeDelta report_window;

    ASSIGN_OR_RETURN(
        report_window,
        ParseLegacyDuration(
            *singular_window,
            SourceRegistrationError::kEventReportWindowValueInvalid));

    return EventReportWindows::CreateSingularWindow(report_window);
  }
  if (multiple_windows) {
    return EventReportWindows::ParseWindowsJSON(*multiple_windows);
  }

  return absl::nullopt;
}

base::expected<EventReportWindows, SourceRegistrationError>
EventReportWindows::ParseWindowsJSON(const base::Value& v) {
  const base::Value::Dict* dict = v.GetIfDict();
  if (!dict) {
    return base::unexpected(
        SourceRegistrationError::kEventReportWindowsWrongType);
  }

  base::TimeDelta start_time = base::Seconds(0);
  if (const base::Value* start_time_value = dict->Find(kStartTime)) {
    absl::optional<int> int_value = start_time_value->GetIfInt();
    if (!int_value.has_value()) {
      return base::unexpected(
          SourceRegistrationError::kEventReportWindowsStartTimeWrongType);
    }
    if (*int_value < 0) {
      return base::unexpected(
          SourceRegistrationError::kEventReportWindowsStartTimeInvalid);
    }
    start_time = base::Seconds(*int_value);
  }

  const base::Value* end_times_value = dict->Find(kEndTimes);
  if (!end_times_value) {
    return base::unexpected(
        SourceRegistrationError::kEventReportWindowsEndTimesMissing);
  }

  const base::Value::List* end_times_list = end_times_value->GetIfList();
  if (!end_times_list) {
    return base::unexpected(
        SourceRegistrationError::kEventReportWindowsEndTimesWrongType);
  }

  std::vector<base::TimeDelta> end_times;
  if (end_times_list->empty()) {
    return base::unexpected(
        SourceRegistrationError::kEventReportWindowsEndTimesListEmpty);
  }
  if (end_times_list->size() > kMaxEventLevelReportWindows) {
    return base::unexpected(
        SourceRegistrationError::kEventReportWindowsEndTimesListTooLong);
  }
  end_times.reserve(end_times_list->size());

  base::TimeDelta start_duration = start_time;
  for (const auto& item : *end_times_list) {
    const absl::optional<int> item_int = item.GetIfInt();
    if (!item_int.has_value()) {
      return base::unexpected(
          SourceRegistrationError::kEventReportWindowsEndTimeValueWrongType);
    }
    if (item_int.value() <= 0) {
      return base::unexpected(
          SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid);
    }

    auto end_time = base::Seconds(*item_int);
    if (end_time < kMinReportWindow) {
      end_time = kMinReportWindow;
    }

    if (end_time <= start_duration) {
      return base::unexpected(
          SourceRegistrationError::kEventReportWindowsEndTimeDurationLTEStart);
    }
    end_times.push_back(end_time);
    start_duration = end_time;
  }

  return EventReportWindows(start_time, std::move(end_times));
}

void EventReportWindows::Serialize(base::Value::Dict& dict) const {
  if (OnlySingularWindow()) {
    SerializeTimeDeltaInSeconds(dict, kEventReportWindow,
                                start_time_or_window_time_);
  } else {
    base::Value::Dict windows_dict;

    windows_dict.Set(kStartTime,
                     static_cast<int>(start_time_or_window_time_.InSeconds()));

    base::Value::List list;
    for (const auto& end_time : end_times_) {
      list.Append(static_cast<int>(end_time.InSeconds()));
    }

    windows_dict.Set(kEndTimes, std::move(list));
    dict.Set(kEventReportWindows, std::move(windows_dict));
  }
}

}  // namespace attribution_reporting
