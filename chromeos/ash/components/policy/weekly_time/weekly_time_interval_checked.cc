// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"

#include <optional>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"

namespace policy {

namespace {

constexpr base::TimeDelta kWeek = base::Days(7);

}  // namespace

// static
const char WeeklyTimeIntervalChecked::kStart[] = "start";
const char WeeklyTimeIntervalChecked::kEnd[] = "end";

WeeklyTimeIntervalChecked::WeeklyTimeIntervalChecked(
    const WeeklyTimeChecked& start,
    const WeeklyTimeChecked& end)
    : start_(start), end_(end) {}

WeeklyTimeIntervalChecked::WeeklyTimeIntervalChecked(
    const WeeklyTimeIntervalChecked&) = default;

WeeklyTimeIntervalChecked& WeeklyTimeIntervalChecked::operator=(
    const WeeklyTimeIntervalChecked&) = default;

// static
bool WeeklyTimeIntervalChecked::IntervalsOverlap(
    const WeeklyTimeIntervalChecked& a,
    const WeeklyTimeIntervalChecked& b) {
  return a.Contains(b.start()) || b.Contains(a.start());
}

// static
std::optional<WeeklyTimeIntervalChecked> WeeklyTimeIntervalChecked::FromDict(
    const base::Value::Dict& dict) {
  const base::Value* start_value = dict.Find(kStart);
  if (!start_value) {
    LOG(ERROR) << "Missing start.";
    return std::nullopt;
  }

  const base::Value* end_value = dict.Find(kEnd);
  if (!end_value) {
    LOG(ERROR) << "Missing end.";
    return std::nullopt;
  }

  auto start = WeeklyTimeChecked::FromDict(start_value->GetDict());
  if (!start) {
    LOG(ERROR) << "Couldn't parse start: "
               << start_value->GetDict().DebugString();
    return std::nullopt;
  }

  auto end = WeeklyTimeChecked::FromDict(end_value->GetDict());
  if (!end) {
    LOG(ERROR) << "Couldn't parse end: " << end_value->GetDict().DebugString();
    return std::nullopt;
  }

  return WeeklyTimeIntervalChecked(*start, *end);
}

base::TimeDelta WeeklyTimeIntervalChecked::Duration() const {
  auto diff = end_.ToTimeDelta() - start_.ToTimeDelta();
  if (diff.is_negative() || diff.is_zero()) {
    diff += base::Days(7);
  }
  return diff;
}

bool WeeklyTimeIntervalChecked::Contains(const WeeklyTimeChecked& w) const {
  if (WeeklyTimeIntervalChecked(start_, w).Duration() == kWeek) {
    return true;
  }
  return WeeklyTimeIntervalChecked(start_, w).Duration() +
             WeeklyTimeIntervalChecked(w, end_).Duration() ==
         Duration();
}

}  // namespace policy
