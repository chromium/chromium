// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_uploader.h"
#include "components/policy/core/common/policy_logger.h"
#include "net/base/backoff_entry.h"

namespace enterprise_reporting {

namespace {

const net::BackoffEntry::Policy kRetryPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Minutes(1).InMilliseconds(),
    .multiply_factor = 5.0,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = base::Minutes(5).InMilliseconds(),
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = true,
};

const int kMaxAttempts = 5;

}  // namespace

BrowserLaunchEventController::BrowserLaunchEventController(
    std::unique_ptr<LaunchDataCollector> collector,
    std::unique_ptr<BrowserLaunchEventUploader> uploader)
    : collector_(std::move(collector)),
      uploader_(std::move(uploader)),
      retry_backoff_(&kRetryPolicy) {
  CHECK(collector_);
  CHECK(uploader_);
}

BrowserLaunchEventController::~BrowserLaunchEventController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserLaunchEventController::CollectAndUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!pending_upload_event_.has_value());

  pending_upload_event_ = collector_->GetEvent();

  AttemptUpload();
}

void BrowserLaunchEventController::AttemptUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!retry_timer_.IsRunning());

  uploader_->UploadEvent(
      *pending_upload_event_,
      base::BindOnce(&BrowserLaunchEventController::OnEventUploaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserLaunchEventController::OnEventUploaded(
    policy::CloudPolicyClient::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.IsSuccess()) {
    return;
  }

  // Only retry on transient errors. Permanent errors (e.g. 400 Bad Request,
  // 401 Unauthorized) are likely caused by client bugs or misconfiguration
  // where retrying won't help.
  switch (result.GetDMServerError()) {
    case policy::DM_STATUS_REQUEST_FAILED:         // Network error
    case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:  // 5xx server error
    case policy::DM_STATUS_SERVICE_TOO_MANY_REQUESTS:
      break;
    default:
      LOG_POLICY(ERROR, REPORTING)
          << "Browser launch event upload failed with non-retryable status: "
          << result.GetDMServerError();
      return;
  }

  retry_backoff_.InformOfRequest(false);

  if (retry_backoff_.failure_count() >= kMaxAttempts) {
    LOG_POLICY(ERROR, REPORTING)
        << "Browser launch event upload failed after " << kMaxAttempts
        << " attempts. Giving up. Last failure status: "
        << result.GetDMServerError();
    return;
  }

  base::TimeDelta delay = retry_backoff_.GetTimeUntilRelease();
  LOG_POLICY(WARNING, REPORTING)
      << "Browser launch event upload failed with status: "
      << result.GetDMServerError() << ". Retrying in " << delay;
  retry_timer_.Start(FROM_HERE, delay, this,
                     &BrowserLaunchEventController::AttemptUpload);
}

}  // namespace enterprise_reporting
