// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReportingService handles uploading serialized logs to a server.

#include "components/metrics/reporting_service.h"

#include <cstdio>
#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/data_use_tracker.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_features.h"
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
                                   size_t max_retransmit_size,
                                   MetricsLogsEventManager* logs_event_manager)
    : client_(client),
      local_state_(local_state),
      max_retransmit_size_(max_retransmit_size),
      logs_event_manager_(logs_event_manager),
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
  base::RepeatingClosure send_next_log_callback = base::BindRepeating(
      &ReportingService::SendNextLog, self_ptr_factory_.GetWeakPtr());
  bool fast_startup_for_testing = client_->ShouldStartUpFastForTesting();
  upload_scheduler_ = std::make_unique<MetricsUploadScheduler>(
      send_next_log_callback, fast_startup_for_testing);
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

  // Check whether the log should be uploaded based on user id. If it should not
  // be sent, then discard the log from the store and notify the scheduler.
  auto staged_user_id = log_store()->staged_log_user_id();
  if (staged_user_id.has_value() &&
      !client_->ShouldUploadMetricsForUserId(staged_user_id.value())) {
    // Remove the log and update list to disk.
    log_store()->DiscardStagedLog();
    log_store()->TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);

    // Notify the scheduler that the next log should be uploaded. If there are
    // no more logs, then stop the scheduler.
    if (!log_store()->has_unsent_logs()) {
      DVLOG(1) << "Stopping upload_scheduler_.";
      upload_scheduler_->Stop();
    }
    upload_scheduler_->UploadFinished(true);
    return;
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
        base::BindRepeating(&ReportingService::OnLogUploadComplete,
                            self_ptr_factory_.GetWeakPtr()));
  }

  reporting_info_.set_attempt_count(reporting_info_.attempt_count() + 1);

  const std::string hash =
      base::HexEncode(log_store()->staged_log_hash().data(),
                      log_store()->staged_log_hash().size());
  std::string signature;
  base::Base64Encode(log_store()->staged_log_signature(), &signature);

  if (logs_event_manager_) {
    logs_event_manager_->NotifyLogEvent(
        MetricsLogsEventManager::LogEvent::kLogUploading,
        log_store()->staged_log_hash());
  }
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
    const std::string& staged_log = log_store()->staged_log();
    const size_t log_size = staged_log.length();
    if (upload_succeeded) {
      LogSuccessLogSize(log_size);
      LogSuccessMetadata(staged_log);
    } else if (log_size > max_retransmit_size_) {
      LogLargeRejection(log_size);
      discard_log = true;
    } else if (response_code == 400) {
      // Bad syntax.  Retransmission won't work.
      discard_log = true;
    }

    if (!upload_succeeded && !discard_log && logs_event_manager_) {
      // The log failed to upload and we did not discard it. We will try to
      // retransmit.
      logs_event_manager_->NotifyLogEvent(
          MetricsLogsEventManager::LogEvent::kLogStaged,
          log_store()->staged_log_hash(),
          "Failed to upload. Staged again for retransmission.");
    }

    if (upload_succeeded || discard_log) {
      if (upload_succeeded)
        log_store()->MarkStagedLogAsSent();

      log_store()->DiscardStagedLog();
      // Store the updated list to disk now that the removed log is uploaded.
      log_store()->TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      // If Chrome is in the background, flush the discarded and trimmed logs
      // from |local_state_| immediately because the process may be killed at
      // any time from now without persisting the changes. Otherwise, we may end
      // up re-uploading the same log in a future session. We do not do this if
      // Chrome is in the foreground because of the assumption that
      // |local_state_| will be flushed when convenient, and we do not want to
      // do more work than necessary on the main thread while Chrome is visible.
      if (base::FeatureList::IsEnabled(
              features::kReportingServiceFlushPrefsOnUploadInBackground) &&
          !is_in_foreground_) {
        local_state_->CommitPendingWrite();
      }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
