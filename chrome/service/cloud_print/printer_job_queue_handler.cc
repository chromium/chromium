// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/printer_job_queue_handler.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/values.h"

namespace cloud_print {

namespace {

class TimeProviderImpl : public PrinterJobQueueHandler::TimeProvider {
 public:
  base::Time GetNow() override;
};

base::Time TimeProviderImpl::GetNow() {
  return base::Time::Now();
}

JobDetails ConstructJobDetailsFromJson(const base::Value& job_data) {
  DCHECK(job_data.is_dict());

  JobDetails job_details;
  const std::string* str = job_data.FindStringKey(kIdValue);
  if (str)
    job_details.job_id_ = *str;
  str = job_data.FindStringKey(kTitleValue);
  if (str)
    job_details.job_title_ = *str;
  str = job_data.FindStringKey(kOwnerValue);
  if (str)
    job_details.job_owner_ = *str;
  str = job_data.FindStringKey(kTicketUrlValue);
  if (str)
    job_details.print_ticket_url_ = *str;
  str = job_data.FindStringKey(kFileUrlValue);
  if (str)
    job_details.print_data_url_ = *str;

  // Get tags for print job.
  const base::Value* tags =
      job_data.FindKeyOfType(kTagsValue, base::Value::Type::LIST);
  if (tags) {
    for (const auto& tag : tags->GetList()) {
      if (tag.is_string())
        job_details.tags_.push_back(tag.GetString());
    }
  }
  return job_details;
}

}  // namespace

JobDetails::JobDetails() {}

JobDetails::JobDetails(const JobDetails& other) = default;

JobDetails::~JobDetails() {}

void JobDetails::Clear() {
  job_id_.clear();
  job_title_.clear();
  job_owner_.clear();
  print_ticket_.clear();
  print_ticket_mime_type_.clear();
  print_data_mime_type_.clear();
  print_data_file_path_ = base::FilePath();
  print_data_url_.clear();
  print_ticket_url_.clear();
  tags_.clear();
  time_remaining_ = base::TimeDelta();
}

// static
bool JobDetails::Ordering(const JobDetails& first, const JobDetails& second) {
  return first.time_remaining_ < second.time_remaining_;
}

PrinterJobQueueHandler::PrinterJobQueueHandler(
    std::unique_ptr<TimeProvider> time_provider)
    : time_provider_(std::move(time_provider)) {}

PrinterJobQueueHandler::PrinterJobQueueHandler()
    : time_provider_(new TimeProviderImpl) {}

PrinterJobQueueHandler::~PrinterJobQueueHandler() {}

base::TimeDelta PrinterJobQueueHandler::ComputeBackoffTime(
    const std::string& job_id) {
  FailedJobMap::const_iterator job_location = failed_job_map_.find(job_id);
  if (job_location == failed_job_map_.end()) {
    return base::TimeDelta();
  }

  base::TimeDelta backoff_time = base::Seconds(kJobFirstWaitTimeSecs);
  backoff_time *=
      // casting argument to double and result to uint64_t to avoid compilation
      // issues
      static_cast<int64_t>(
          pow(static_cast<long double>(kJobWaitTimeExponentialMultiplier),
              job_location->second.retries_) +
          0.5);
  base::Time scheduled_retry =
      job_location->second.last_retry_ + backoff_time;
  base::Time now = time_provider_->GetNow();

  if (scheduled_retry < now) {
    return base::TimeDelta();
  }
  return scheduled_retry - now;
}

std::vector<JobDetails> PrinterJobQueueHandler::GetJobsFromQueue(
    const base::Value& json_data) {
  DCHECK(json_data.is_dict());

  std::vector<JobDetails> jobs;

  const base::Value* job_list =
      json_data.FindKeyOfType(kJobListValue, base::Value::Type::LIST);
  if (!job_list)
    return jobs;

  std::vector<JobDetails> jobs_with_timeouts;
  for (const auto& job_value : job_list->GetList()) {
    if (!job_value.is_dict())
      continue;

    JobDetails job_details_current = ConstructJobDetailsFromJson(job_value);
    job_details_current.time_remaining_ =
        ComputeBackoffTime(job_details_current.job_id_);
    if (job_details_current.time_remaining_.is_zero()) {
      jobs.push_back(job_details_current);
    } else {
      jobs_with_timeouts.push_back(job_details_current);
    }
  }

  sort(jobs_with_timeouts.begin(), jobs_with_timeouts.end(),
       &JobDetails::Ordering);
  jobs.insert(jobs.end(), jobs_with_timeouts.begin(), jobs_with_timeouts.end());
  return jobs;
}

void PrinterJobQueueHandler::JobDone(const std::string& job_id) {
  failed_job_map_.erase(job_id);
}

bool PrinterJobQueueHandler::JobFetchFailed(const std::string& job_id) {
  FailedJobMetadata metadata;
  metadata.retries_ = 0;
  metadata.last_retry_ = time_provider_->GetNow();

  std::pair<FailedJobMap::iterator, bool> job_found =
      failed_job_map_.insert(FailedJobPair(job_id, metadata));

  // If the job has already failed once, increment the number of retries.
  // If it has failed too many times, remove it from the map and tell the caller
  // to report a failure.
  if (!job_found.second) {
    if (job_found.first->second.retries_ >= kNumRetriesBeforeAbandonJob) {
      failed_job_map_.erase(job_found.first);
      return false;
    }

    job_found.first->second.retries_ += 1;
    job_found.first->second.last_retry_ = time_provider_->GetNow();
  }

  return true;
}

}  // namespace cloud_print
