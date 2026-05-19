// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_uploader.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
// Retry starts with 1 minute delay and is doubled with every failure.
const net::BackoffEntry::Policy kDefaultReportUploadBackoffPolicy = {
    0,  // Number of initial errors to ignore before applying
        // exponential back-off rules.
    base::Minutes(2).InMilliseconds(),  // Initial delay
    2,     // Factor by which the waiting time will be multiplied.
    0.1,   // Fuzzing percentage.
    -1,    // No maximum delay.
    -1,    // It's up to the caller to reset the backoff time.
    false  // Do not always use initial delay.
};

void RecordReportResponseMetrics(ReportResponseMetricsStatus status) {
  base::UmaHistogramEnumeration("Enterprise.CloudReportingResponse", status);
}

enum class EnterpriseCloudReportingPolicyStatus {
  kNoPolicySet = 0,
  kUserCloudPolicySetOnly = 1,
  kOtherPolicySetOnly = 2,
  kBothPolicySet = 3,
  kMaxValue = kBothPolicySet
};

void RecordProfilePolicyStatus(const em::ChromeProfileReportRequest& request,
                               SecuritySignalsMode security_signals_mode) {
  if (!request.has_browser_report() ||
      request.browser_report().chrome_user_profile_infos_size() == 0) {
    return;
  }
  DCHECK_EQ(request.browser_report().chrome_user_profile_infos_size(), 1);

  bool has_user_cloud_policy = false;
  bool has_other_policy = false;

  const auto& profile_info =
      request.browser_report().chrome_user_profile_infos(0);
  for (const auto& policy : profile_info.chrome_policies()) {
    if (policy.source() == em::Policy_PolicySource_SOURCE_MERGED) {
      for (const auto& conflict : policy.conflicts()) {
        if (conflict.source() == em::Policy_PolicySource_SOURCE_CLOUD &&
            conflict.scope() == em::Policy_PolicyScope_SCOPE_USER) {
          has_user_cloud_policy = true;
        } else {
          has_other_policy = true;
        }
      }
    } else {
      if (policy.source() == em::Policy_PolicySource_SOURCE_CLOUD &&
          policy.scope() == em::Policy_PolicyScope_SCOPE_USER) {
        has_user_cloud_policy = true;
      } else {
        has_other_policy = true;
      }
    }
    if (has_user_cloud_policy && has_other_policy) {
      break;
    }
  }

  EnterpriseCloudReportingPolicyStatus status;
  if (has_user_cloud_policy && has_other_policy) {
    status = EnterpriseCloudReportingPolicyStatus::kBothPolicySet;
  } else if (has_user_cloud_policy) {
    status = EnterpriseCloudReportingPolicyStatus::kUserCloudPolicySetOnly;
  } else if (has_other_policy) {
    status = EnterpriseCloudReportingPolicyStatus::kOtherPolicySetOnly;
  } else {
    status = EnterpriseCloudReportingPolicyStatus::kNoPolicySet;
  }

  std::string mode_str;
  switch (security_signals_mode) {
    case SecuritySignalsMode::kNoSignals:
      mode_str = "NoSignals";
      break;
    case SecuritySignalsMode::kSignalsAttached:
      mode_str = "SignalsAttached";
      break;
    case SecuritySignalsMode::kSignalsOnly:
      mode_str = "SignalsOnly";
      break;
  }

  base::UmaHistogramEnumeration(
      "Enterprise.CloudReportingPolicyStatus.Profile." + mode_str, status);
}

}  // namespace

ReportUploader::ReportUploader(policy::CloudPolicyClient* client,
                               int maximum_number_of_retries)
    : client_(client),
      backoff_entry_(&kDefaultReportUploadBackoffPolicy),
      maximum_number_of_retries_(maximum_number_of_retries) {}
ReportUploader::~ReportUploader() = default;

void ReportUploader::SetRequestAndUpload(const ReportGenerationConfig& config,
                                         ReportRequestQueue requests,
                                         ReportCallback callback) {
  config_ = config;
  requests_ = std::move(requests);
  callback_ = std::move(callback);
  Upload();
}

void ReportUploader::Upload() {
  auto callback = base::BindRepeating(&ReportUploader::OnRequestFinished,
                                      weak_ptr_factory_.GetWeakPtr());

  switch (config_.report_type) {
    case ReportType::kFull:
    case ReportType::kBrowserVersion: {
      auto request = std::make_unique<ReportRequest::DeviceReportRequestProto>(
          requests_.front()->GetDeviceReportRequest());
      // Because MessageLite does not support DebugMessage(), print
      // serialize string for debugging purposes. It's a non-human-friendly
      // binary string but still provide useful information.
      VLOG(2) << "Uploading report: " << request->SerializeAsString();

#if BUILDFLAG(IS_CHROMEOS)
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

      if (config_.security_signals_mode != SecuritySignalsMode::kNoSignals) {
        VLOG_POLICY(1, REPORTING)
            << "Uploading profile report with signals mode "
            << static_cast<int>(config_.security_signals_mode);
      }

      RecordProfilePolicyStatus(*request, config_.security_signals_mode);

      client_->UploadChromeProfileReport(
          config_.use_cookies, std::move(request), std::move(callback));
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
