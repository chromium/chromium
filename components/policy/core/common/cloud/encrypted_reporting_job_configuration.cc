// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
    base::Value merging_payload,
    UploadCompleteCallback complete_cb)
    : ReportingJobConfigurationBase(TYPE_UPLOAD_ENCRYPTED_REPORT,
                                    client->GetURLLoaderFactory(),
                                    client,
                                    server_url,
                                    /*include_device_info*/ true,
                                    std::move(complete_cb)) {
  // Merge it into the base class payload.
  payload_.MergeDictionary(&merging_payload);
}

EncryptedReportingJobConfiguration::~EncryptedReportingJobConfiguration() {
  if (!callback_.is_null()) {
    // The job either wasn't tried, or failed in some unhandled way. Report
    // failure to the callback.
    std::move(callback_).Run(/*job=*/nullptr,
                             DeviceManagementStatus::DM_STATUS_REQUEST_FAILED,
                             /*net_error=*/418,
                             /*response_body=*/base::Value());
  }
}

void EncryptedReportingJobConfiguration::UpdatePayloadBeforeGetInternal() {
  // Can't mutate payload_ and iterate it at the same time, so build a
  // disallowed list and then remove the values.
  std::set<std::string> disallowed_keys;
  for (auto key_value_pair : payload_.DictItems()) {
    if (GetTopLevelKeyAllowList().count(key_value_pair.first) == 0) {
      disallowed_keys.insert(key_value_pair.first);
    }
  }

  for (auto key : disallowed_keys) {
    payload_.RemoveKey(key);
  }
}

void EncryptedReportingJobConfiguration::UpdateContext(base::Value& context) {
  context_ = std::move(context);
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
