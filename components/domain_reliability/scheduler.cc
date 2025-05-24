// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/scheduler.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/util.h"
#include "net/base/backoff_entry.h"

namespace {

const unsigned kInvalidCollectorIndex = static_cast<unsigned>(-1);

const unsigned kDefaultMinimumUploadDelaySec = 60;
const unsigned kDefaultMaximumUploadDelaySec = 300;
const unsigned kDefaultUploadRetryIntervalSec = 60;

const char* kMinimumUploadDelayFieldTrialName = "DomRel-MinimumUploadDelay";
const char* kMaximumUploadDelayFieldTrialName = "DomRel-MaximumUploadDelay";
const char* kUploadRetryIntervalFieldTrialName = "DomRel-UploadRetryInterval";

// Fixed elements of backoff policy
const double kMultiplyFactor = 2.0;
const double kJitterFactor = 0.1;
const int64_t kMaximumBackoffMs = 60 * 1000 * 1000;

unsigned GetUnsignedFieldTrialValueOrDefault(std::string field_trial_name,
                                             unsigned default_value) {
  if (!base::FieldTrialList::TrialExists(field_trial_name))
    return default_value;

  std::string group_name = base::FieldTrialList::FindFullName(field_trial_name);
  unsigned value;
  if (!base::StringToUint(group_name, &value)) {
    LOG(ERROR) << "Expected unsigned integer for field trial "
               << field_trial_name << " group name, but got \"" << group_name
               << "\".";
    return default_value;
  }

  return value;
}

}  // namespace

namespace domain_reliability {

// static
DomainReliabilityScheduler::Params
DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults() {
  DomainReliabilityScheduler::Params params;

  params.minimum_upload_delay =
      base::Seconds(GetUnsignedFieldTrialValueOrDefault(
          kMinimumUploadDelayFieldTrialName, kDefaultMinimumUploadDelaySec));
  params.maximum_upload_delay =
      base::Seconds(GetUnsignedFieldTrialValueOrDefault(
          kMaximumUploadDelayFieldTrialName, kDefaultMaximumUploadDelaySec));
  params.upload_retry_interval =
      base::Seconds(GetUnsignedFieldTrialValueOrDefault(
          kUploadRetryIntervalFieldTrialName, kDefaultUploadRetryIntervalSec));

  return params;
}

DomainReliabilityScheduler::DomainReliabilityScheduler(
    const MockableTime* time,
    size_t num_collectors,
    const Params& params,
    const ScheduleUploadCallback& callback)
    : time_(time),
      params_(params),
      callback_(callback),
      upload_pending_(false),
      upload_scheduled_(false),
      upload_running_(false),
      collector_index_(kInvalidCollectorIndex) {
  backoff_policy_.num_errors_to_ignore = 0;
  backoff_policy_.initial_delay_ms =
      params.upload_retry_interval.InMilliseconds();
  backoff_policy_.multiply_factor = kMultiplyFactor;
  backoff_policy_.jitter_factor = kJitterFactor;
  backoff_policy_.maximum_backoff_ms = kMaximumBackoffMs;
  backoff_policy_.entry_lifetime_ms = 0;
  backoff_policy_.always_use_initial_delay = false;

  for (size_t i = 0; i < num_collectors; ++i) {
    collectors_.push_back(std::make_unique<net::BackoffEntry>(
        &backoff_policy_, time->AsTickClock()));
  }
}

DomainReliabilityScheduler::~DomainReliabilityScheduler() = default;

void DomainReliabilityScheduler::OnBeaconAdded() {
  if (!upload_pending_)
    first_beacon_time_ = time_->NowTicks();
  upload_pending_ = true;
  MaybeScheduleUpload();
}

size_t DomainReliabilityScheduler::OnUploadStart() {
  DCHECK(upload_scheduled_);
  DCHECK_EQ(kInvalidCollectorIndex, collector_index_);
  upload_pending_ = false;
  upload_scheduled_ = false;
  upload_running_ = true;

  base::TimeTicks now = time_->NowTicks();
  base::TimeTicks min_upload_time;
  GetNextUploadTimeAndCollector(now, &min_upload_time, &collector_index_);
  DCHECK(min_upload_time <= now);

  VLOG(1) << "Starting upload to collector " << collector_index_ << ".";

  return collector_index_;
}

void DomainReliabilityScheduler::OnUploadComplete(
    const DomainReliabilityUploader::UploadResult& result) {
  DCHECK(upload_running_);
  DCHECK_NE(kInvalidCollectorIndex, collector_index_);
  upload_running_ = false;

  VLOG(1) << "Upload to collector " << collector_index_
          << (result.is_success() ? " succeeded." : " failed.");

  net::BackoffEntry* backoff = collectors_[collector_index_].get();
  collector_index_ = kInvalidCollectorIndex;

  backoff->InformOfRequest(result.is_success());
  if (result.is_retry_after())
    backoff->SetCustomReleaseTime(time_->NowTicks() + result.retry_after);

  if (!result.is_success()) {
    // Restore upload_pending_ and first_beacon_time_ to pre-upload state,
    // since upload failed.
    upload_pending_ = true;
    first_beacon_time_ = old_first_beacon_time_;
  }

  MaybeScheduleUpload();
}

void DomainReliabilityScheduler::MakeDeterministicForTesting() {
  backoff_policy_.jitter_factor = 0.0;
}

void DomainReliabilityScheduler::MaybeScheduleUpload() {
  if (!upload_pending_ || upload_scheduled_ || upload_running_)
    return;

  upload_scheduled_ = true;
  old_first_beacon_time_ = first_beacon_time_;

  base::TimeTicks now = time_->NowTicks();

  base::TimeTicks min_by_deadline, max_by_deadline;
  min_by_deadline = first_beacon_time_ + params_.minimum_upload_delay;
  max_by_deadline = first_beacon_time_ + params_.maximum_upload_delay;
  DCHECK(min_by_deadline <= max_by_deadline);

  base::TimeTicks min_by_backoff;
  size_t collector_index;
  GetNextUploadTimeAndCollector(now, &min_by_backoff, &collector_index);

  base::TimeTicks scheduled_min_time =
      std::max(min_by_deadline, min_by_backoff);
  base::TimeTicks scheduled_max_time =
      std::max(max_by_deadline, min_by_backoff);

  base::TimeDelta min_delay = scheduled_min_time - now;
  base::TimeDelta max_delay = scheduled_max_time - now;

  VLOG(1) << "Scheduling upload for between " << min_delay.InSeconds()
          << " and " << max_delay.InSeconds() << " seconds from now.";

  callback_.Run(min_delay, max_delay);
}

// TODO(juliatuttle): Add min and max interval to config, use that instead.

// TODO(juliatuttle): Cap min and max intervals received from config.

void DomainReliabilityScheduler::GetNextUploadTimeAndCollector(
    base::TimeTicks now,
    base::TimeTicks* upload_time_out,
    size_t* collector_index_out) {
  DCHECK(upload_time_out);
  DCHECK(collector_index_out);

  base::TimeTicks min_time;
  size_t min_index = kInvalidCollectorIndex;

  for (size_t i = 0; i < collectors_.size(); ++i) {
    net::BackoffEntry* backoff = collectors_[i].get();
    // If a collector is usable, use the first one in the list.
    if (!backoff->ShouldRejectRequest()) {
      min_time = now;
      min_index = i;
      break;
    }

    // If not, keep track of which will be usable soonest:
    base::TimeTicks time = backoff->GetReleaseTime();
    if (min_index == kInvalidCollectorIndex || time < min_time) {
      min_time = time;
      min_index = i;
    }
  }

  DCHECK_NE(kInvalidCollectorIndex, min_index);
  *upload_time_out = min_time;
  *collector_index_out = min_index;
}

}  // namespace domain_reliability
