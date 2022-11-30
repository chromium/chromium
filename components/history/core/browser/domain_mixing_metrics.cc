// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/domain_mixing_metrics.h"

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"

namespace history {
namespace {

// For readability, represents days as times rounded down to a multiple in
// days from begin_time.
using Day = base::Time;
using DomainVisits = base::flat_map<std::string, int>;
using DomainVisitsPerDay = base::flat_map<Day, DomainVisits>;

// The time intervals in days to compute domain mixing metrics for, sorted
// in ascending order.
std::vector<int> NumDaysForMetrics() {
  return {kOneDay, kOneWeek, kTwoWeeks, kOneMonth};
}

// Maps a time to the start of a day using ref_start_of_day as the reference
// time for the start of a day. Some examples:
//
// time = 2018-01-02 13:00:00 UTC
// ref = 2018-01-04 04:00:00 UTC
// result = 2018-01-02 04:00:00 UTC
//
// time = 2018-01-06 03:00:00 UTC
// ref = 2018-01-04 04:00:00 UTC
// result = 2018-01-05 04:00:00 UTC
Day ToStartOfDay(base::Time time, Day ref_start_of_day) {
  return ref_start_of_day +
         base::Days((time - ref_start_of_day).InDaysFloored());
}

// Counts the number of visits per day and per domain as a nested map
// day -> domain -> num_visits.
// start_of_day is used as the reference time for the start of a day.
DomainVisitsPerDay CountDomainVisitsPerDay(
    base::Time start_of_day,
    const std::vector<DomainVisit>& domain_visits) {
  DomainVisitsPerDay domain_visits_per_day;
  for (const DomainVisit& visit : domain_visits) {
    const Day day = ToStartOfDay(visit.visit_time(), start_of_day);
    DomainVisits& domain_visits_for_day = domain_visits_per_day[day];
    ++domain_visits_for_day[visit.domain()];
  }
  return domain_visits_per_day;
}

// Computes the domain mixing ratio given the number of visits for each domain.
double ComputeDomainMixingRatio(const DomainVisits& domain_visits) {
  // First, we extract the domain with the most visits.
  const auto top_domain = std::max_element(
      domain_visits.begin(), domain_visits.end(),
      [](const DomainVisits::value_type& a, const DomainVisits::value_type& b) {
        return a.second < b.second;
      });

  // Then we compute the number of visits that are not on the top domain
  // (secondary domains).
  int other_visits = 0;
  for (const auto& domain_num_visits : domain_visits) {
    if (domain_num_visits.first != top_domain->first)
      other_visits += domain_num_visits.second;
  }

  // Finally, we compute the domain mixing ratio which is the ratio of the
  // number of visits on secondary domains to the total number of visits.
  // This ratio is equal to 0 if all visits are on the top domain (no domain
  // mixing) and is close to 1 if most visits are on secondary domains.
  DCHECK_GT(other_visits + top_domain->second, 0)
      << "Tried to compute domain mixing for a time range with no domain "
         "visits, this should never happen as we only compute domain mixing "
         "for active days.";
  return static_cast<double>(other_visits) /
         (other_visits + top_domain->second);
}

void EmitDomainMixingMetric(const DomainVisits& domain_visits, int num_days) {
  double domain_mixing_ratio = ComputeDomainMixingRatio(domain_visits);
  int percentage = base::ClampRound(100 * domain_mixing_ratio);
  switch (num_days) {
    case kOneDay:
      UMA_HISTOGRAM_PERCENTAGE("DomainMixing.OneDay", percentage);
      break;
    case kOneWeek:
      UMA_HISTOGRAM_PERCENTAGE("DomainMixing.OneWeek", percentage);
      break;
    case kTwoWeeks:
      UMA_HISTOGRAM_PERCENTAGE("DomainMixing.TwoWeeks", percentage);
      break;
    case kOneMonth:
      UMA_HISTOGRAM_PERCENTAGE("DomainMixing.OneMonth", percentage);
      break;
    default:
      // This should never happen.
      NOTREACHED();
  }
}

void EmitDomainMixingMetricsForDay(
    const DomainVisitsPerDay::const_iterator& active_day,
    const DomainVisitsPerDay& domain_visits_per_day) {
  DomainVisits domain_visits = active_day->second;
  // To efficiently compute domain mixing for each of the time periods, we
  // aggregate domain visits preceding the active day in a single pass.
  // The metrics to emit are sorted by increasing time period lengths.
  // We take them in order, aggregate the number of activity days required
  // for the current one, then move on to the next one.
  // Reverse iterator, starting at the day before active_day.
  auto it = std::make_reverse_iterator(active_day);
  for (const int num_days : NumDaysForMetrics()) {
    const Day first_day = active_day->first - base::Days(num_days - 1);
    for (; it != domain_visits_per_day.rend() && it->first >= first_day; ++it) {
      for (const auto& domain_num_visits : it->second) {
        domain_visits[domain_num_visits.first] += domain_num_visits.second;
      }
    }
    // We have aggregated all the days within the time window for the current
    // metric.
    EmitDomainMixingMetric(domain_visits, num_days);
  }
}

}  // namespace

void EmitDomainMixingMetrics(const std::vector<DomainVisit>& domain_visits,
                             base::Time start_of_first_day_to_emit) {
  // We count the visits per domain for each day of user activity.
  DomainVisitsPerDay domain_visits_per_day =
      CountDomainVisitsPerDay(start_of_first_day_to_emit, domain_visits);

  // We then compute domain mixing metrics for each day of activity within
  // [start_of_first_day_to_emit, last_day].
  for (auto active_day_it =
           domain_visits_per_day.lower_bound(start_of_first_day_to_emit);
       active_day_it != domain_visits_per_day.end(); ++active_day_it) {
    EmitDomainMixingMetricsForDay(active_day_it, domain_visits_per_day);
  }
}

}  // namespace history
