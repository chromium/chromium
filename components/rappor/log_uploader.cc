// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/log_uploader.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

// The delay, in seconds, between uploading when there are queued logs to send.
const int kUnsentLogsIntervalSeconds = 3;

// When uploading metrics to the server fails, we progressively wait longer and
// longer before sending the next log. This backoff process helps reduce load
// on a server that is having issues.
// The following is the multiplier we use to expand that inter-log duration.
const double kBackoffMultiplier = 1.1;

// The maximum backoff multiplier.
const int kMaxBackoffIntervalSeconds = 60 * 60;

// The maximum number of unsent logs we will keep.
// TODO(holte): Limit based on log size instead.
const size_t kMaxQueuedLogs = 10;

enum DiscardReason {
  UPLOAD_SUCCESS,
  UPLOAD_REJECTED,
  QUEUE_OVERFLOW,
  NUM_DISCARD_REASONS
};

void RecordDiscardReason(DiscardReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Rappor.DiscardReason",
                            reason,
                            NUM_DISCARD_REASONS);
}

}  // namespace

namespace rappor {

LogUploader::LogUploader(
    const GURL& server_url,
    const std::string& mime_type,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : server_url_(server_url),
      mime_type_(mime_type),
      url_loader_factory_(std::move(url_loader_factory)),
      is_running_(false),
      has_callback_pending_(false),
      upload_interval_(
          base::TimeDelta::FromSeconds(kUnsentLogsIntervalSeconds)) {}

LogUploader::~LogUploader() {}

void LogUploader::Start() {
  is_running_ = true;
  StartScheduledUpload();
}

void LogUploader::Stop() {
  is_running_ = false;
  // Rather than interrupting the current upload, just let it finish/fail and
  // then inhibit any retry attempts.
}

void LogUploader::QueueLog(const std::string& log) {
  queued_logs_.push(log);
  // Don't drop logs yet if an upload is in progress.  They will be dropped
  // when it finishes.
  if (!has_callback_pending_)
    DropExcessLogs();
  StartScheduledUpload();
}

void LogUploader::DropExcessLogs() {
  while (queued_logs_.size() > kMaxQueuedLogs) {
    DVLOG(2) << "Dropping excess log.";
    RecordDiscardReason(QUEUE_OVERFLOW);
    queued_logs_.pop();
  }
}

bool LogUploader::IsUploadScheduled() const {
  return upload_timer_.IsRunning();
}

void LogUploader::ScheduleNextUpload(base::TimeDelta interval) {
  upload_timer_.Start(
      FROM_HERE, interval, this, &LogUploader::StartScheduledUpload);
}

bool LogUploader::CanStartUpload() const {
  return is_running_ &&
         !queued_logs_.empty() &&
         !IsUploadScheduled() &&
         !has_callback_pending_;
}

void LogUploader::StartScheduledUpload() {
  if (!CanStartUpload())
    return;
  DVLOG(2) << "Upload to " << server_url_.spec() << " starting.";
  has_callback_pending_ = true;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("rappor_report", R"(
        semantics {
          sender: "RAPPOR"
          description:
            "This service sends RAPPOR anonymous usage statistics to Google."
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Chromium is running."
          data: "A protocol buffer with RAPPOR anonymous usage statistics."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by stopping "
            "'Automatically send usage statistics and crash reports to Google'"
            "in Chromium's settings under Advanced Settings, Privacy. The "
            "feature is enabled by default."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = server_url_;
  // We already drop cookies server-side, but we might as well strip them out
  // client-side as well.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(queued_logs_.front(), mime_type_);
  // TODO re-add data use measurement once SimpleURLLoader supports it.
  // ID=data_use_measurement::DataUseUserData::RAPPOR
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&LogUploader::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

// static
base::TimeDelta LogUploader::BackOffUploadInterval(base::TimeDelta interval) {
  DCHECK_GT(kBackoffMultiplier, 1.0);
  interval = base::TimeDelta::FromMicroseconds(
      static_cast<int64_t>(kBackoffMultiplier * interval.InMicroseconds()));

  base::TimeDelta max_interval =
      base::TimeDelta::FromSeconds(kMaxBackoffIntervalSeconds);
  return interval > max_interval ? max_interval : interval;
}

void LogUploader::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  DVLOG(2) << "Upload fetch complete response code: " << response_code;

  int net_error = simple_url_loader_->NetError();
  if (net_error != net::OK && (response_code == -1 || response_code == 200)) {
    base::UmaHistogramSparse("Rappor.FailedUploadErrorCode", -net_error);
    DVLOG(1) << "Rappor server upload failed with error: " << net_error << ": "
             << net::ErrorToString(net_error);
  } else {
    // Log a histogram to track response success vs. failure rates.
    base::UmaHistogramSparse("Rappor.UploadResponseCode", response_code);
  }

  const bool upload_succeeded = !!response_body;

  // Determine whether this log should be retransmitted.
  DiscardReason reason = NUM_DISCARD_REASONS;
  if (upload_succeeded) {
    reason = UPLOAD_SUCCESS;
  } else if (response_code == 400) {
    reason = UPLOAD_REJECTED;
  }

  if (reason != NUM_DISCARD_REASONS) {
    DVLOG(2) << "Log discarded.";
    RecordDiscardReason(reason);
    queued_logs_.pop();
  }

  DropExcessLogs();

  // Error 400 indicates a problem with the log, not with the server, so
  // don't consider that a sign that the server is in trouble.
  const bool server_is_healthy = upload_succeeded || response_code == 400;
  OnUploadFinished(server_is_healthy);
}

void LogUploader::OnUploadFinished(bool server_is_healthy) {
  DCHECK(has_callback_pending_);
  has_callback_pending_ = false;
  // If the server is having issues, back off. Otherwise, reset to default.
  if (!server_is_healthy)
    upload_interval_ = BackOffUploadInterval(upload_interval_);
  else
    upload_interval_ = base::TimeDelta::FromSeconds(kUnsentLogsIntervalSeconds);

  if (CanStartUpload())
    ScheduleNextUpload(upload_interval_);
}

}  // namespace rappor
