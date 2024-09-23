// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_report_windows.h"

#include <stddef.h>

#include <functional>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/source_type.mojom-shared.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;

bool IsValid(base::TimeDelta start_time,
             const base::flat_set<base::TimeDelta>& end_times) {
  return !start_time.is_negative() && !end_times.empty() &&
         end_times.size() <= kMaxEventLevelReportWindows &&
         *end_times.begin() > start_time &&
         *end_times.begin() >= kMinReportWindow;
}

bool IsReportWindowValid(base::TimeDelta report_window) {
  return report_window >= kMinReportWindow && report_window <= kMaxSourceExpiry;
}

bool IsStrictlyIncreasing(const std::vector<base::TimeDelta>& end_times) {
  return base::ranges::adjacent_find(end_times, std::not_fn(std::less{})) ==
         end_times.end();
}

base::Time ReportTimeFromDeadline(base::Time source_time,
                                  base::TimeDelta deadline) {
  // Valid conversion reports should always have a valid reporting deadline.
  CHECK(deadline.is_positive());
  return source_time + deadline;
}

}  // namespace

// static
std::optional<EventReportWindows> EventReportWindows::FromDefaults(
    base::TimeDelta report_window,
    SourceType source_type) {
  if (!IsReportWindowValid(report_window)) {
    return std::nullopt;
  }
  return EventReportWindows(report_window, source_type);
}

// static
std::optional<EventReportWindows> EventReportWindows::Create(
    base::TimeDelta start_time,
    std::vector<base::TimeDelta> end_times) {
  if (!IsStrictlyIncreasing(end_times)) {
    return std::nullopt;
  }
  base::flat_set<base::TimeDelta> end_times_set(base::sorted_unique,
                                                std::move(end_times));
  if (!IsValid(start_time, end_times_set)) {
    return std::nullopt;
  }
  return EventReportWindows(start_time, std::move(end_times_set));
}

EventReportWindows::EventReportWindows(
    base::TimeDelta start_time,
    base::flat_set<base::TimeDelta> end_times)
    : start_time_(start_time), end_times_(std::move(end_times)) {
  CHECK(IsValid(start_time_, end_times_));
}

EventReportWindows::EventReportWindows(base::TimeDelta report_window,
                                       SourceType source_type) {
  CHECK(IsReportWindowValid(report_window));

  std::vector<base::TimeDelta> end_times;
  switch (source_type) {
    case SourceType::kNavigation:
      end_times = {base::Days(2), base::Days(7)};
      break;
    case SourceType::kEvent:
      break;
  }

  while (!end_times.empty() && report_window <= end_times.back()) {
    end_times.pop_back();
  }

  end_times.push_back(report_window);
  end_times_.replace(std::move(end_times));
  CHECK(IsValid(start_time_, end_times_));
}

EventReportWindows::EventReportWindows()
    : EventReportWindows(/*start_time=*/base::TimeDelta(),
                         /*end_times=*/{kMaxSourceExpiry}) {}

EventReportWindows::~EventReportWindows() = default;

EventReportWindows::EventReportWindows(const EventReportWindows&) = default;

EventReportWindows& EventReportWindows::operator=(const EventReportWindows&) =
    default;

EventReportWindows::EventReportWindows(EventReportWindows&&) = default;

EventReportWindows& EventReportWindows::operator=(EventReportWindows&&) =
    default;

// Follows the steps detailed in
// https://wicg.github.io/attribution-reporting-api/#obtain-an-event-level-report-delivery-time
// Starting from step 2.
base::Time EventReportWindows::ComputeReportTime(
    base::Time source_time,
    base::Time trigger_time) const {
  // It is possible for a source to have an assigned time of T and a trigger
  // that is attributed to it to have a time of T-X e.g. due to user-initiated
  // clock changes.
  //
  // TODO(crbug.com/40282914): Assume `source_time` is smaller than
  // `trigger_time` once attribution time resolution is implemented in storage.
  const base::Time trigger_time_floored =
      source_time < trigger_time ? trigger_time
                                 : source_time + base::Microseconds(1);
  base::TimeDelta reporting_window_to_use = *end_times_.rbegin();

  for (base::TimeDelta reporting_window : end_times_) {
    if (source_time + reporting_window <= trigger_time_floored) {
      continue;
    }
    reporting_window_to_use = reporting_window;
    break;
  }
  return ReportTimeFromDeadline(source_time, reporting_window_to_use);
}

base::Time EventReportWindows::ReportTimeAtWindow(base::Time source_time,
                                                  int window_index) const {
  CHECK_GE(window_index, 0);
  CHECK_LT(static_cast<size_t>(window_index), end_times_.size());

  return ReportTimeFromDeadline(source_time,
                                *std::next(end_times_.begin(), window_index));
}

base::Time EventReportWindows::StartTimeAtWindow(base::Time source_time,
                                                 int window_index) const {
  CHECK_GE(window_index, 0);
  CHECK_LT(static_cast<size_t>(window_index), end_times_.size());

  if (window_index == 0) {
    return source_time + start_time_;
  }

  return source_time + *std::next(end_times_.begin(), window_index - 1);
}

EventReportWindows::WindowResult EventReportWindows::FallsWithin(
    base::TimeDelta trigger_moment) const {
  // It is possible for a source to have an assigned time of T and a trigger
  // that is attributed to it to have a time of T-X e.g. due to user-initiated
  // clock changes.
  //
  // TODO(crbug.com/40283992): Assume trigger moment is not negative once
  // attribution time resolution is implemented in storage.
  base::TimeDelta bounded_trigger_moment =
      trigger_moment.is_negative() ? base::Microseconds(0) : trigger_moment;

  if (bounded_trigger_moment < start_time_) {
    return WindowResult::kNotStarted;
  }
  if (bounded_trigger_moment >= *end_times_.rbegin()) {
    return WindowResult::kPassed;
  }
  return WindowResult::kFallsWithin;
}

base::expected<EventReportWindows, SourceRegistrationError>
EventReportWindows::FromJSON(const base::Value::Dict& registration,
                             base::TimeDelta expiry,
                             SourceType source_type) {
  const base::Value* singular_window = registration.Find(kEventReportWindow);
  const base::Value* multiple_windows = registration.Find(kEventReportWindows);

  base::UmaHistogramBoolean("Conversions.LegacyEventReportWindow",
                            !!singular_window);

  if (singular_window && multiple_windows) {
    return base::unexpected(
        SourceRegistrationError::kBothEventReportWindowFieldsFound);
  } else if (singular_window) {
    ASSIGN_OR_RETURN(
        base::TimeDelta report_window,
        ParseLegacyDuration(*singular_window,
                            /*clamp_min=*/kMinReportWindow,
                            /*clamp_max=*/expiry),
        [](ParseError) {
          return SourceRegistrationError::kEventReportWindowValueInvalid;
        });

    return EventReportWindows(report_window, source_type);
  } else if (multiple_windows) {
    return ParseWindowsJSON(*multiple_windows, expiry);
  } else {
    return EventReportWindows(expiry, source_type);
  }
}

// static
base::expected<EventReportWindows, SourceRegistrationError>
EventReportWindows::ParseWindows(const base::Value::Dict& dict,
                                 base::TimeDelta expiry,
                                 const EventReportWindows& default_if_absent) {
  const base::Value* value = dict.Find(kEventReportWindows);
  if (!value) {
    return default_if_absent;
  }
  return ParseWindowsJSON(*value, expiry);
}

// static
base::expected<EventReportWindows, SourceRegistrationError>
EventReportWindows::ParseWindowsJSON(const base::Value& v,
                                     base::TimeDelta expiry) {
  const base::Value::Dict* dict = v.GetIfDict();
  if (!dict) {
    return base::unexpected(
        SourceRegistrationError::kEventReportWindowsWrongType);
  }

  base::TimeDelta start_time = base::Seconds(0);
  if (const base::Value* start_time_value = dict->Find(kStartTime)) {
    ASSIGN_OR_RETURN(
        int int_value, ParseInt(*start_time_value), [](ParseError) {
          return SourceRegistrationError::kEventReportWindowsStartTimeInvalid;
        });
    start_time = base::Seconds(int_value);
    if (start_time.is_negative() || start_time > expiry) {
      return base::unexpected(
          SourceRegistrationError::kEventReportWindowsStartTimeInvalid);
    }
  }

  const base::Value* end_times_value = dict->Find(kEndTimes);
  if (!end_times_value) {
    return base::unexpected(
        SourceRegistrationError::kEventReportWindowsEndTimesMissing);
  }

  const base::Value::List* end_times_list = end_times_value->GetIfList();
  if (!end_times_list || end_times_list->empty() ||
      end_times_list->size() > kMaxEventLevelReportWindows) {
    return base::unexpected(
        SourceRegistrationError::kEventReportWindowsEndTimesListInvalid);
  }

  std::vector<base::TimeDelta> end_times;
  end_times.reserve(end_times_list->size());

  base::TimeDelta start_duration = start_time;
  for (const auto& item : *end_times_list) {
    ASSIGN_OR_RETURN(base::TimeDelta end_time, ParseDuration(item),
                     [](ParseError) {
                       return SourceRegistrationError::
                           kEventReportWindowsEndTimeValueInvalid;
                     });

    if (!end_time.is_positive()) {
      return base::unexpected(
          SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid);
    }

    if (end_time > expiry) {
      end_time = expiry;
    }
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
  base::Value::Dict windows_dict;

  windows_dict.Set(kStartTime, static_cast<int>(start_time_.InSeconds()));

  auto list = base::Value::List::with_capacity(end_times_.size());
  for (const auto& end_time : end_times_) {
    list.Append(static_cast<int>(end_time.InSeconds()));
  }

  windows_dict.Set(kEndTimes, std::move(list));
  dict.Set(kEventReportWindows, std::move(windows_dict));
}

bool EventReportWindows::IsValidForExpiry(base::TimeDelta expiry) const {
  return start_time_ <= expiry && *end_times_.rbegin() <= expiry;
}

}  // namespace attribution_reporting
