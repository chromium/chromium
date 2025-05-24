// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/disconnectable_client.h"

#include <limits>
#include <memory>
#include <queue>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"

namespace reporting {

// The feature is enabled by default, to be disabled as a kill switch only
// (if it is disabled, all jobs are permitted to run, no limit)
BASE_FEATURE(kEnableReportingDelegateJobsLimit,
             "EnableReportingDelegateJobsLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr char kMaxDelegatesRunningParamName[] = "max_running";

const base::FeatureParam<size_t> kMaxDelegatesRunning{
    &kEnableReportingDelegateJobsLimit, kMaxDelegatesRunningParamName, 10};

namespace {

size_t max_delegate_jobs_running() {
  if (!base::FeatureList::IsEnabled(kEnableReportingDelegateJobsLimit)) {
    return std::numeric_limits<size_t>::max();
  }
  return kMaxDelegatesRunning.Get();
}

}  // namespace

DisconnectableClient::DisconnectableClient(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {}

DisconnectableClient::~DisconnectableClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetAvailability(/*is_available=*/false);
}

// static
const base::Feature& DisconnectableClient::ReportingDelegateJobsLimitFeature() {
  return kEnableReportingDelegateJobsLimit;
}

// static
const char* DisconnectableClient::MaxDelegatesRunningParamName() {
  return kMaxDelegatesRunningParamName;
}

void DisconnectableClient::MaybeMakeCall(std::unique_ptr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Bail out, if missive daemon is not available over dBus.
  if (!is_available_) {
    delegate->Respond(Status(error::UNAVAILABLE,
                             disconnectable_client::kErrorServiceUnavailable));
    base::UmaHistogramEnumeration(
        reporting::kUmaUnavailableErrorReason,
        UnavailableErrorReason::CLIENT_NOT_CONNECTED_TO_MISSIVE,
        UnavailableErrorReason::MAX_VALUE);
    return;
  }

  // Check whether there are too many delegates running already.
  if (outstanding_delegates_.size() >= max_delegate_jobs_running()) {
    // Delay.
    delayed_delegates_.push(std::move(delegate));
    return;
  }

  // Otherwise call it.
  MakeCall(std::move(delegate));
}

void DisconnectableClient::MakeCall(std::unique_ptr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Bail out, if missive daemon is not available over dBus.
  if (!is_available_) {
    delegate->Respond(Status(error::UNAVAILABLE,
                             disconnectable_client::kErrorServiceUnavailable));
    base::UmaHistogramEnumeration(
        reporting::kUmaUnavailableErrorReason,
        UnavailableErrorReason::CLIENT_NOT_CONNECTED_TO_MISSIVE,
        UnavailableErrorReason::MAX_VALUE);
    return;
  }
  // Add the delegate to the map.
  const auto id = ++last_id_;
  auto res = outstanding_delegates_.emplace(id, std::move(delegate));
  // Make a call, resume on CallResponded, when response is received.
  res.first->second->DoCall(base::BindOnce(&DisconnectableClient::CallResponded,
                                           weak_ptr_factory_.GetWeakPtr(), id));
}

void DisconnectableClient::CallResponded(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = outstanding_delegates_.find(id);
  if (it == outstanding_delegates_.end()) {
    // Callback has already been removed, no action needed.
    return;
  }
  // Remove delegate from |outstanding_delegates_|.
  const auto delegate = std::move(it->second);
  outstanding_delegates_.erase(it);
  // Respond through the |delegate|.
  delegate->Respond(Status::StatusOK());
  // If there are more delegates in queue, run the first one or more.
  // Note that we check the count again, since `max_delegate_jobs_running`
  // could have changed.
  while (!delayed_delegates_.empty() &&
         outstanding_delegates_.size() < max_delegate_jobs_running()) {
    MakeCall(std::move(delayed_delegates_.front()));
    delayed_delegates_.pop();
  }
}

void DisconnectableClient::SetAvailability(bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_available_ = is_available;
  LOG(WARNING) << "Service became " << (is_available_ ? "" : "un")
               << "available";
  if (is_available_) {
    // Start delayed calls, if there are spots.
    while (!delayed_delegates_.empty() &&
           outstanding_delegates_.size() < max_delegate_jobs_running()) {
      MakeCall(std::move(delayed_delegates_.front()));
      delayed_delegates_.pop();
    }
  } else {
    // Cancel all pending calls.
    while (!outstanding_delegates_.empty()) {
      // Remove the first delegate from |outstanding_delegates_|.
      const auto delegate = std::move(outstanding_delegates_.begin()->second);
      outstanding_delegates_.erase(outstanding_delegates_.begin());
      // Respond through the |delegate|.
      delegate->Respond(Status(
          error::UNAVAILABLE, disconnectable_client::kErrorServiceUnavailable));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::CLIENT_NOT_CONNECTED_TO_MISSIVE,
          UnavailableErrorReason::MAX_VALUE);
    }
    // Cancel all delayed calls.
    while (!delayed_delegates_.empty()) {
      // Remove the first delegate from |delayed_delegates_|.
      const auto delegate = std::move(delayed_delegates_.front());
      delayed_delegates_.pop();
      // Respond through the |delegate|.
      delegate->Respond(Status(
          error::UNAVAILABLE, disconnectable_client::kErrorServiceUnavailable));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::CLIENT_NOT_CONNECTED_TO_MISSIVE,
          UnavailableErrorReason::MAX_VALUE);
    }
  }
}

}  // namespace reporting
