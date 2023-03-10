// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/metrics/domain_diversity_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {
// The interval between two successive domain metrics reports.
constexpr base::TimeDelta kDomainDiversityReportingInterval = base::Days(1);

// Pref name for the persistent timestamp of the last report. This pref is
// per local profile but not synced.
constexpr char kDomainDiversityReportingTimestamp[] =
    "domain_diversity.last_reporting_timestamp";
}  // namespace

DomainDiversityReporter::DomainDiversityReporter(
    history::HistoryService* history_service,
    PrefService* prefs,
    base::Clock* clock)
    : history_service_(history_service),
      prefs_(prefs),
      clock_(clock),
      history_service_observer_(this) {
  DCHECK_NE(prefs_, nullptr);
  DCHECK_NE(history_service_, nullptr);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DomainDiversityReporter::MaybeComputeDomainMetrics,
                     weak_ptr_factory_.GetWeakPtr()));
}

DomainDiversityReporter::~DomainDiversityReporter() = default;

// static
void DomainDiversityReporter::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(kDomainDiversityReportingTimestamp, base::Time());
}

void DomainDiversityReporter::MaybeComputeDomainMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (history_service_->BackendLoaded()) {
    // HistoryService is ready; proceed to start the domain metrics
    // computation task.
    ComputeDomainMetrics();
  }
  // Observe history service and start reporting as soon as
  // the former is ready.
  DCHECK(!history_service_observer_.IsObservingSource(history_service_.get()));
  history_service_observer_.Observe(history_service_.get());
}

void DomainDiversityReporter::ComputeDomainMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time time_last_report_triggered =
      prefs_->GetTime(kDomainDiversityReportingTimestamp);
  base::Time time_current_report_triggered = clock_->Now();

  if (time_last_report_triggered < time_current_report_triggered) {
    // The lower boundary of all times is set at Unix epoch, since
    // LocalMidnight() may fail on times represented by a very small value
    // (e.g. Windows epoch).
    if (time_last_report_triggered < base::Time::UnixEpoch())
      time_last_report_triggered = base::Time::UnixEpoch();

    if (time_current_report_triggered < base::Time::UnixEpoch())
      time_current_report_triggered = base::Time::UnixEpoch();

    // Will only report up to 7 days x 3 results.
    int number_of_days_to_report = 7;

    // If the last report time is too far back in the past, simply use the
    // highest possible value for `number_of_days_to_report` and skip its
    // computation. This avoids calling LocalMidnight() on some very old
    // timestamp that may cause unexpected behaviors on certain
    // platforms/timezones (see https://crbug.com/1048145).
    // The beginning and the end of a 7-day period may differ by at most
    // 24 * 8 + 1(DST offset) hours; round up to FromDays(9) here.
    if (time_current_report_triggered - time_last_report_triggered <
        base::Days(number_of_days_to_report + 2)) {
      // Compute the number of days that needs to be reported for based on
      // the last report time and current time.
      base::TimeDelta report_time_range =
          time_current_report_triggered.LocalMidnight() -
          time_last_report_triggered.LocalMidnight();

      // Due to daylight saving time, `report_time_range` may not be a multiple
      // of 24 hours. A small time offset is therefore added to
      // `report_time_range` so that the resulting time range is guaranteed to
      // be at least the correct number of days times 24. The number of days to
      // report is capped at 7 days.
      number_of_days_to_report =
          std::min((report_time_range + base::Hours(4)).InDaysFloored(),
                   number_of_days_to_report);
    }

    if (number_of_days_to_report >= 1) {
      history_service_->GetDomainDiversity(
          /*report_time=*/time_current_report_triggered,
          /*number_of_days_to_report=*/number_of_days_to_report,
          /*metric_type_bitmask=*/history::kEnableLast1DayMetric |
              history::kEnableLast7DayMetric | history::kEnableLast28DayMetric,
          base::BindOnce(&DomainDiversityReporter::ReportDomainMetrics,
                         weak_ptr_factory_.GetWeakPtr(),
                         time_current_report_triggered),
          &cancelable_task_tracker_);
    }
  }

  // The next reporting task is scheduled to run 24 hours later.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DomainDiversityReporter::ComputeDomainMetrics,
                     weak_ptr_factory_.GetWeakPtr()),
      kDomainDiversityReportingInterval);
}

void DomainDiversityReporter::ReportDomainMetrics(
    base::Time time_current_report_triggered,
    std::pair<history::DomainDiversityResults, history::DomainDiversityResults>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // An empty DomainDiversityResults indicates that `db_` is null in
  // HistoryBackend.
  if (result.first.empty() && result.second.empty()) {
    return;
  }

  // `result.first` is the "local" result (excluding synced visits), while
  // `result.second` is the "all" result, including both local and synced
  // visits.

  for (auto& result_one_day : result.first) {
    base::UmaHistogramCounts1000("History.DomainCount1Day_V3",
                                 result_one_day.one_day_metric.value().count);
    base::UmaHistogramCounts1000("History.DomainCount7Day_V3",
                                 result_one_day.seven_day_metric.value().count);
    base::UmaHistogramCounts1000(
        "History.DomainCount28Day_V3",
        result_one_day.twenty_eight_day_metric.value().count);
  }

  for (auto& result_one_day : result.second) {
    base::UmaHistogramCounts1000("History.DomainCount1Day_V2",
                                 result_one_day.one_day_metric.value().count);
    base::UmaHistogramCounts1000("History.DomainCount7Day_V2",
                                 result_one_day.seven_day_metric.value().count);
    base::UmaHistogramCounts1000(
        "History.DomainCount28Day_V2",
        result_one_day.twenty_eight_day_metric.value().count);
  }

  prefs_->SetTime(kDomainDiversityReportingTimestamp,
                  time_current_report_triggered);
}

void DomainDiversityReporter::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  DCHECK_EQ(history_service, history_service_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ComputeDomainMetrics();
}

void DomainDiversityReporter::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  history_service_observer_.Reset();
  cancelable_task_tracker_.TryCancelAll();
}
