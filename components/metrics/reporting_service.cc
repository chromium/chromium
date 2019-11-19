// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReportingService handles uploading serialized logs to a server.

#include "components/metrics/reporting_service.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/data_use_tracker.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_upload_scheduler.h"

namespace metrics {

// static
void ReportingService::RegisterPrefs(PrefRegistrySimple* registry) {
  DataUseTracker::RegisterPrefs(registry);
}

ReportingService::ReportingService(MetricsServiceClient* client,
                                   PrefService* local_state,
                                   size_t max_retransmit_size)
    : client_(client),
      max_retransmit_size_(max_retransmit_size),
      reporting_active_(false),
      log_upload_in_progress_(false),
      data_use_tracker_(DataUseTracker::Create(local_state)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_);
  DCHECK(local_state);
}

ReportingService::~ReportingService() {
  DisableReporting();
}

void ReportingService::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!upload_scheduler_);
  log_store()->LoadPersistedUnsentLogs();
  base::Closure send_next_log_callback = base::Bind(
      &ReportingService::SendNextLog, self_ptr_factory_.GetWeakPtr());
  bool fast_startup_for_testing = client_->ShouldStartUpFastForTesting();
  upload_scheduler_.reset(new MetricsUploadScheduler(send_next_log_callback,
                                                     fast_startup_for_testing));
}

void ReportingService::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (reporting_active_)
    upload_scheduler_->Start();
}

void ReportingService::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (upload_scheduler_)
    upload_scheduler_->Stop();
}

void ReportingService::EnableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (reporting_active_)
    return;
  reporting_active_ = true;
  Start();
}

void ReportingService::DisableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reporting_active_ = false;
  Stop();
}

bool ReportingService::reporting_active() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return reporting_active_;
}

//------------------------------------------------------------------------------
// private methods
//------------------------------------------------------------------------------

void ReportingService::SendNextLog() {
  DVLOG(1) << "SendNextLog";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::TimeTicks now = base::TimeTicks::Now();
  LogActualUploadInterval(last_upload_finish_time_.is_null()
                              ? base::TimeDelta()
                              : now - last_upload_finish_time_);
  last_upload_finish_time_ = now;

  if (!reporting_active()) {
    upload_scheduler_->StopAndUploadCancelled();
    return;
  }
  if (!log_store()->has_unsent_logs()) {
    // Should only get here if serializing the log failed somehow.
    upload_scheduler_->Stop();
    // Reset backoff interval
    upload_scheduler_->UploadFinished(true);
    return;
  }
  if (!log_store()->has_staged_log()) {
    reporting_info_.set_attempt_count(0);
    log_store()->StageNextLog();
  }

  // Proceed to stage the log for upload if log size satisfies cellular log
  // upload constrains.
  bool upload_canceled = false;
  bool is_cellular_logic = client_->IsUMACellularUploadLogicEnabled();
  if (is_cellular_logic && data_use_tracker_ &&
      !data_use_tracker_->ShouldUploadLogOnCellular(
          log_store()->staged_log().size())) {
    upload_scheduler_->UploadOverDataUsageCap();
    upload_canceled = true;
  } else {
    SendStagedLog();
  }
  if (is_cellular_logic) {
    LogCellularConstraint(upload_canceled);
  }
}

void ReportingService::SendStagedLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(log_store()->has_staged_log());
  if (!log_store()->has_staged_log())
    return;

  DCHECK(!log_upload_in_progress_);
  log_upload_in_progress_ = true;

  if (!log_uploader_) {
    log_uploader_ = client_->CreateUploader(
        GetUploadUrl(), GetInsecureUploadUrl(), upload_mime_type(),
        service_type(),
        base::Bind(&ReportingService::OnLogUploadComplete,
                   self_ptr_factory_.GetWeakPtr()));
  }

  reporting_info_.set_attempt_count(reporting_info_.attempt_count() + 1);

  const std::string hash =
      base::HexEncode(log_store()->staged_log_hash().data(),
                      log_store()->staged_log_hash().size());
  std::string signature;
  base::Base64Encode(log_store()->staged_log_signature(), &signature);
  log_uploader_->UploadLog(log_store()->staged_log(), hash, signature,
                           reporting_info_);
}

void ReportingService::OnLogUploadComplete(int response_code,
                                           int error_code,
                                           bool was_https) {
  DVLOG(1) << "OnLogUploadComplete:" << response_code;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(log_upload_in_progress_);
  log_upload_in_progress_ = false;

  reporting_info_.set_last_response_code(response_code);
  reporting_info_.set_last_error_code(error_code);
  reporting_info_.set_last_attempt_was_https(was_https);

  // Log a histogram to track response success vs. failure rates.
  LogResponseOrErrorCode(response_code, error_code, was_https);

  bool upload_succeeded = response_code == 200;

  // Staged log could have been removed already (such as by Purge() in some
  // implementations), otherwise we may remove it here.
  if (log_store()->has_staged_log()) {
    // Provide boolean for error recovery (allow us to ignore response_code).
    bool discard_log = false;
    const size_t log_size = log_store()->staged_log().length();
    if (upload_succeeded) {
      LogSuccess(log_size);
    } else if (log_size > max_retransmit_size_) {
      LogLargeRejection(log_size);
      discard_log = true;
    } else if (response_code == 400) {
      // Bad syntax.  Retransmission won't work.
      discard_log = true;
    }

    if (upload_succeeded || discard_log) {
      log_store()->DiscardStagedLog();
      // Store the updated list to disk now that the removed log is uploaded.
      log_store()->PersistUnsentLogs();
    }
  }

  // Error 400 indicates a problem with the log, not with the server, so
  // don't consider that a sign that the server is in trouble.
  bool server_is_healthy = upload_succeeded || response_code == 400;

  if (!log_store()->has_unsent_logs()) {
    DVLOG(1) << "Stopping upload_scheduler_.";
    upload_scheduler_->Stop();
  }
  upload_scheduler_->UploadFinished(server_is_healthy);
}

}  // namespace metrics
