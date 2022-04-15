// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/data_collection_scheduler.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

namespace segmentation_platform {

namespace {
// Minimum intervals between collection.
// TODO(qinmin): make this configurable through finch.
static int kMinimumReportingIntervalInHours = 12;

// Given the last report time, calculate the next report time.
base::Time GetNextReportTime(base::Time last_report_time) {
  // The next report time is determined by the next day after the last
  // report or the |kMinimumReportingIntervalInHours| hours after last report,
  // whichever arrived later.
  base::Time next_day =
      base::Time() +
      base::Days(1 + (last_report_time - base::Time()).InDaysFloored());
  base::Time next_allowed_report_time =
      last_report_time + base::Hours(kMinimumReportingIntervalInHours);

  return next_day > next_allowed_report_time ? next_day
                                             : next_allowed_report_time;
}

}  // namespace

DataCollectionScheduler::DataCollectionScheduler(
    TrainingDataCollector* training_data_collector,
    PrefService* prefs,
    base::Clock* clock)
    : training_data_collector_(training_data_collector),
      prefs_(prefs),
      clock_(clock) {}

DataCollectionScheduler::~DataCollectionScheduler() = default;

void DataCollectionScheduler::ReportTrainingDataIfApplicable() {
  base::Time last_collection_time =
      prefs_->GetTime(kSegmentationLastCollectionTimePref);
  base::Time now = clock_->Now();

  base::Time next_collection_time = GetNextReportTime(last_collection_time);
  base::TimeDelta delay_to_next_collection = next_collection_time - now;
  if (now >= next_collection_time) {
    training_data_collector_->ReportCollectedContinuousTrainingData();
    delay_to_next_collection = GetNextReportTime(now) - now;
  }

  // Schedule a task for the next report.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DataCollectionScheduler::ReportTrainingDataIfApplicable,
                     weak_ptr_factory_.GetWeakPtr()),
      delay_to_next_collection);
}

}  // namespace segmentation_platform