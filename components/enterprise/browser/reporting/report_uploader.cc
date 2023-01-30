// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_uploader.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
// Retry starts with 1 minute delay and is doubled with every failure.
const net::BackoffEntry::Policy kDefaultReportUploadBackoffPolicy = {
    0,      // Number of initial errors to ignore before applying
            // exponential back-off rules.
    60000,  // Initial delay is 60 seconds.
    2,      // Factor by which the waiting time will be multiplied.
    0.1,    // Fuzzing percentage.
    -1,     // No maximum delay.
    -1,     // It's up to the caller to reset the backoff time.
    false   // Do not always use initial delay.
};

void RecordReportResponseMetrics(ReportResponseMetricsStatus status) {
  base::UmaHistogramEnumeration("Enterprise.CloudReportingResponse", status);
}

}  // namespace

ReportUploader::ReportUploader(policy::CloudPolicyClient* client,
                               int maximum_number_of_retries)
    : client_(client),
      backoff_entry_(&kDefaultReportUploadBackoffPolicy),
      maximum_number_of_retries_(maximum_number_of_retries) {}
ReportUploader::~ReportUploader() = default;

void ReportUploader::SetRequestAndUpload(ReportType report_type,
                                         ReportRequestQueue requests,
                                         ReportCallback callback) {
  report_type_ = report_type;
  requests_ = std::move(requests);
  callback_ = std::move(callback);
  Upload();
}

void ReportUploader::Upload() {
  auto callback = base::BindRepeating(&ReportUploader::OnRequestFinished,
                                      weak_ptr_factory_.GetWeakPtr());

  switch (report_type_) {
    case ReportType::kFull:
    case ReportType::kBrowserVersion: {
      auto request = std::make_unique<ReportRequest::DeviceReportRequestProto>(
          requests_.front()->GetDeviceReportRequest());
      // Because MessageLite does not support DebugMessage(), print
      // serialize string for debugging purposes. It's a non-human-friendly
      // binary string but still provide useful information.
      VLOG(2) << "Uploading report: " << request->SerializeAsString();

#if BUILDFLAG(IS_CHROMEOS_ASH)
      client_->UploadChromeOsUserReport(std::move(request),
                                        std::move(callback));
#else
      client_->UploadChromeDesktopReport(std::move(request),
                                         std::move(callback));
#endif
      break;
    }
    case ReportType::kProfileReport: {
      auto request = std::make_unique<em::ChromeProfileReportRequest>(
          requests_.front()->GetChromeProfileReportRequest());
      VLOG(2) << "Uploading report: " << request->SerializeAsString();
      client_->UploadChromeProfileReport(std::move(request),
                                         std::move(callback));
      break;
    }
  }
}

void ReportUploader::OnRequestFinished(
    policy::CloudPolicyClient::Result result) {
  // Crash if the client is not registered, this should not happen.
  // TODO(b/256553070) Handle unregistered case without crashing.
  CHECK(!result.IsClientNotRegisteredError());

  if (result.IsSuccess()) {
    NextRequest();
    RecordReportResponseMetrics(ReportResponseMetricsStatus::kSuccess);
    return;
  }

  switch (result.GetDMServerError()) {
    case policy::DM_STATUS_REQUEST_FAILED:  // network error
      RecordReportResponseMetrics(ReportResponseMetricsStatus::kNetworkError);
      Retry();
      break;
    case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:  // 5xx server error
      RecordReportResponseMetrics(
          ReportResponseMetricsStatus::kTemporaryServerError);
      Retry();
      break;
    // DM_STATUS_SERVICE_DEVICE_ID_CONFLICT is caused by 409 conflict. It can
    // be caused by either device id conflict or DDS concur error which is
    // a database error. We only want to retry for the second case. However,
    // there is no way for us to tell difference right now so we will retry
    // regardless.
    case policy::DM_STATUS_SERVICE_TOO_MANY_REQUESTS:
      RecordReportResponseMetrics(
          ReportResponseMetricsStatus::kDDSConcurrencyError);
      Retry();
      break;
    case policy::DM_STATUS_REQUEST_TOO_LARGE:
      // Treats the REQUEST_TOO_LARGE error as a success upload. It's likely
      // a calculation error during request generating and there is nothing
      // can be done here.
      RecordReportResponseMetrics(
          ReportResponseMetricsStatus::kRequestTooLargeError);
      NextRequest();
      break;
    default:
      RecordReportResponseMetrics(ReportResponseMetricsStatus::kOtherError);
      SendResponse(ReportStatus::kPersistentError);
      break;
  }
}

void ReportUploader::Retry() {
  backoff_entry_.InformOfRequest(false);
  // We have retried enough, time to give up.
  if (HasRetriedTooOften()) {
    SendResponse(ReportStatus::kTransientError);
    return;
  }
  backoff_request_timer_.Start(
      FROM_HERE, backoff_entry_.GetTimeUntilRelease(),
      base::BindOnce(&ReportUploader::Upload, weak_ptr_factory_.GetWeakPtr()));
}

bool ReportUploader::HasRetriedTooOften() {
  return backoff_entry_.failure_count() > maximum_number_of_retries_;
}

void ReportUploader::SendResponse(const ReportStatus status) {
  std::move(callback_).Run(status);
}

void ReportUploader::NextRequest() {
  // We don't reset the backoff in case there are multiple requests in a row
  // and we don't start from 1 minute again.
  backoff_entry_.InformOfRequest(true);
  requests_.pop();
  if (requests_.size() == 0)
    SendResponse(ReportStatus::kSuccess);
  else
    Upload();
  return;
}

}  // namespace enterprise_reporting
