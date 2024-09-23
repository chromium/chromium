// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {
namespace {

DMAuth GetAuthData(const std::string& dm_token) {
  if (dm_token.empty()) {
    // This is the case for uploading managed user events from an unmanaged
    // device. The server will authenticate by looking at the user dm tokens
    // inside the records instead of a single request-level device dm token.
    return DMAuth::NoAuth();
  } else {
    // The device cloud policy client only exists on managed devices and is the
    // source of the DM token. So if the device is managed, we use the device dm
    // token as authentication.
    return DMAuth::FromDMToken(dm_token);
  }
}
}  // namespace

EncryptedReportingJobConfiguration::EncryptedReportingJobConfiguration(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const std::string& server_url,
    base::Value::Dict merging_payload,
    const std::string dm_token,
    const std::string client_id,
    UploadResponseCallback response_cb,
    UploadCompleteCallback complete_cb)
    : ReportingJobConfigurationBase(TYPE_UPLOAD_ENCRYPTED_REPORT,
                                    factory,
                                    GetAuthData(dm_token),
                                    server_url,
                                    std::move(complete_cb)),
      is_device_managed_(!dm_token.empty()),
      response_cb_(std::move(response_cb)) {
  if (is_device_managed_) {
    // Payload for managed device
    InitializePayloadWithDeviceInfo(dm_token, client_id);
  } else {
    // Payload for unmanaged device
    InitializePayloadWithoutDeviceInfo();
  }
  // Merge it into the base class payload.
  payload_.Merge(std::move(merging_payload));
}

EncryptedReportingJobConfiguration::~EncryptedReportingJobConfiguration() {
  if (!callback_.is_null()) {
    // The job either wasn't tried, or failed in some unhandled way. Report
    // failure to the callback.
    std::move(callback_).Run(/*job=*/nullptr,
                             DeviceManagementStatus::DM_STATUS_REQUEST_FAILED,
                             /*response_code=*/418,
                             /*response_body=*/std::nullopt);
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

DeviceManagementService::Job::RetryMethod
EncryptedReportingJobConfiguration::ShouldRetry(
    int response_code,
    const std::string& response_body) {
  // Do not retry on the Job level - ERP has its own retry mechanism.
  return DeviceManagementService::Job::NO_RETRY;
}

void EncryptedReportingJobConfiguration::OnURLLoadComplete(
    DeviceManagementService::Job* job,
    int net_error,
    int response_code,
    const std::string& response_body) {
  // Delegate net error and response code for further analysis that may
  // affect retries and back-off.
  std::move(response_cb_).Run(net_error, response_code);
  // Then forward response to the base class.
  ReportingJobConfigurationBase::OnURLLoadComplete(
      job, net_error, response_code, response_body);
}

std::string EncryptedReportingJobConfiguration::GetUmaString() const {
  if (is_device_managed_) {
    return "Browser.ERP.Managed";
  }
  return "Browser.ERP.Unmanaged";
}

// static
const base::flat_set<std::string>&
EncryptedReportingJobConfiguration::GetTopLevelKeyAllowList() {
  static const base::NoDestructor<base::flat_set<std::string>>
      kTopLevelKeyAllowList{std::initializer_list<std::string>{
          reporting::json_keys::kAttachEncryptionSettings,
          reporting::json_keys::kBrowser,
          reporting::json_keys::kConfigurationFileVersion,
          reporting::json_keys::kDevice,
          reporting::json_keys::kEncryptedRecordList,
          reporting::json_keys::kRequestId, reporting::json_keys::kSource}};
  return *kTopLevelKeyAllowList;
}
}  // namespace policy
