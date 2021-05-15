// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "components/enterprise/common/strings.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace em = enterprise_management;

namespace policy {

const char RealtimeReportingJobConfiguration::kContextKey[] = "context";
const char RealtimeReportingJobConfiguration::kEventListKey[] = "events";

const char RealtimeReportingJobConfiguration::kEventIdKey[] = "eventId";
const char RealtimeReportingJobConfiguration::kUploadedEventsKey[] =
    "uploadedEventIds";
const char RealtimeReportingJobConfiguration::kFailedUploadsKey[] =
    "failedUploads";
const char RealtimeReportingJobConfiguration::kPermanentFailedUploadsKey[] =
    "permanentFailedUploads";

base::Value RealtimeReportingJobConfiguration::BuildReport(
    base::Value events,
    base::Value context) {
  base::Value value_report(base::Value::Type::DICTIONARY);
  value_report.SetKey(kEventListKey, std::move(events));
  value_report.SetKey(kContextKey, std::move(context));
  return value_report;
}

RealtimeReportingJobConfiguration::RealtimeReportingJobConfiguration(
    CloudPolicyClient* client,
    const std::string& server_url,
    bool include_device_info,
    bool add_connector_url_params,
    UploadCompleteCallback callback)
    : ReportingJobConfigurationBase(TYPE_UPLOAD_REAL_TIME_REPORT,
                                    client->GetURLLoaderFactory(),
                                    client,
                                    server_url,
                                    include_device_info,
                                    std::move(callback)) {
  InitializePayloadInternal(client, add_connector_url_params);
}

RealtimeReportingJobConfiguration::~RealtimeReportingJobConfiguration() {}

bool RealtimeReportingJobConfiguration::AddReport(base::Value report) {
  if (!report.is_dict())
    return false;

  absl::optional<base::Value> context_result = report.ExtractKey(kContextKey);
  absl::optional<base::Value> event_list = report.ExtractKey(kEventListKey);
  if (!context_result || !event_list || !event_list->is_list())
    return false;

  // Overwrite internal context. |context_| will be merged with |payload_| in
  // |GetPayload|.
  if (context_.has_value()) {
    context_->MergeDictionary(&context_result.value());
  } else {
    context_ = std::move(context_result);
  }

  // Append event_list to the payload.
  base::Value* to = payload_.FindListKey(kEventListKey);
  for (auto& event : event_list->GetList())
    to->Append(std::move(event));
  return true;
}

void RealtimeReportingJobConfiguration::InitializePayloadInternal(
    CloudPolicyClient* client,
    bool add_connector_url_params) {
  payload_.SetPath(kEventListKey, base::Value(base::Value::Type::LIST));

  // If specified add extra enterprise connector URL params.
  if (add_connector_url_params) {
    AddParameter(enterprise::kUrlParamConnector, "OnSecurityEvent");
    AddParameter(enterprise::kUrlParamDeviceToken, client->dm_token());
  }
}

DeviceManagementService::Job::RetryMethod
RealtimeReportingJobConfiguration::ShouldRetryInternal(
    int response_code,
    const std::string& response_body) {
  DeviceManagementService::Job::RetryMethod retry_method =
      DeviceManagementService::Job::NO_RETRY;
  const auto failedIds = GetFailedUploadIds(response_body);
  if (!failedIds.empty()) {
    retry_method = DeviceManagementService::Job::RETRY_WITH_DELAY;
  }
  return retry_method;
}

void RealtimeReportingJobConfiguration::OnBeforeRetryInternal(
    int response_code,
    const std::string& response_body) {
  const auto& failedIds = GetFailedUploadIds(response_body);
  if (!failedIds.empty()) {
    auto* events = payload_.FindListKey(kEventListKey);
    // Only keep the elements that temporarily failed their uploads.
    events->EraseListValueIf([&failedIds](const base::Value& entry) {
      auto* id = entry.FindStringKey(kEventIdKey);
      return id && failedIds.find(*id) == failedIds.end();
    });
  }
}

std::string RealtimeReportingJobConfiguration::GetUmaString() const {
  return "Enterprise.RealtimeReportingSuccess";
}

std::set<std::string> RealtimeReportingJobConfiguration::GetFailedUploadIds(
    const std::string& response_body) const {
  std::set<std::string> failedIds;
  absl::optional<base::Value> response = base::JSONReader::Read(response_body);
  base::Value response_value = response ? std::move(*response) : base::Value();
  base::Value* failedUploads = response_value.FindListKey(kFailedUploadsKey);
  if (failedUploads) {
    for (const auto& failedUpload : failedUploads->GetList()) {
      auto* id = failedUpload.FindStringKey(kEventIdKey);
      if (id) {
        failedIds.insert(*id);
      }
    }
  }
  return failedIds;
}

}  // namespace policy
