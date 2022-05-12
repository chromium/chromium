// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {

// EncryptedReportingJobConfiguration strings
constexpr char kEncryptedRecordListKey[] = "encryptedRecord";
constexpr char kAttachEncryptionSettingsKey[] = "attachEncryptionSettings";
constexpr char kDeviceKey[] = "device";
constexpr char kBrowserKey[] = "browser";

}  // namespace

EncryptedReportingJobConfiguration::EncryptedReportingJobConfiguration(
    CloudPolicyClient* client,
    const std::string& server_url,
    const base::Value::Dict& merging_payload,
    UploadCompleteCallback complete_cb)
    : ReportingJobConfigurationBase(TYPE_UPLOAD_ENCRYPTED_REPORT,
                                    client->GetURLLoaderFactory(),
                                    client,
                                    server_url,
                                    /*include_device_info*/ true,
                                    std::move(complete_cb)) {
  // Merge it into the base class payload.
  payload_.Merge(merging_payload);
}

EncryptedReportingJobConfiguration::~EncryptedReportingJobConfiguration() {
  if (!callback_.is_null()) {
    // The job either wasn't tried, or failed in some unhandled way. Report
    // failure to the callback.
    std::move(callback_).Run(/*job=*/nullptr,
                             DeviceManagementStatus::DM_STATUS_REQUEST_FAILED,
                             /*response_code=*/418,
                             /*response_body=*/absl::nullopt);
  }
}

void EncryptedReportingJobConfiguration::UpdatePayloadBeforeGetInternal() {
  for (auto it = payload_.begin(); it != payload_.end();) {
    const auto& [key, value] = *it;
    if (!base::Contains(GetTopLevelKeyAllowList(), key)) {
      it = payload_.erase(it);
      continue;
    }
    ++it;
  }
}

void EncryptedReportingJobConfiguration::UpdateContext(
    base::Value::Dict context) {
  context_ = std::move(context);
}

base::TimeDelta EncryptedReportingJobConfiguration::WhenIsAllowedToProceed()
    const {
  return base::TimeDelta();  // 0 - allowed right away. TODO(b/214044545):
                             // implement.
}

void EncryptedReportingJobConfiguration::CancelNotAllowedJob() {
  std::move(callback_).Run(
      /*job=*/nullptr, DeviceManagementStatus::DM_STATUS_REQUEST_FAILED,
      /*response_code=*/DeviceManagementService::kTooManyRequests,
      /*response_body=*/absl::nullopt);
}

void EncryptedReportingJobConfiguration::AccountForAllowedJob() {
  // TODO(b/214044545): implement.
}

DeviceManagementService::Job::RetryMethod
EncryptedReportingJobConfiguration::ShouldRetry(
    int response_code,
    const std::string& response_body) {
  // Do not retry on the Job level - ERP has its own retry mechanism.
  return DeviceManagementService::Job::NO_RETRY;
}

std::string EncryptedReportingJobConfiguration::GetUmaString() const {
  return "Enterprise.EncryptedReportingSuccess";
}

std::set<std::string>
EncryptedReportingJobConfiguration::GetTopLevelKeyAllowList() {
  static std::set<std::string> kTopLevelKeyAllowList{
      kEncryptedRecordListKey, kAttachEncryptionSettingsKey, kDeviceKey,
      kBrowserKey};
  return kTopLevelKeyAllowList;
}

}  // namespace policy
