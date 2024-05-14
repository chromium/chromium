// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"

#include <optional>
#include <utility>

#include "base/json/json_reader.h"
#include "components/enterprise/common/strings.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

base::Value::Dict RealtimeReportingJobConfiguration::BuildReport(
    base::Value::List events,
    base::Value::Dict context) {
  base::Value::Dict value_report;
  value_report.Set(kEventListKey, std::move(events));
  value_report.Set(kContextKey, std::move(context));
  return value_report;
}

RealtimeReportingJobConfiguration::RealtimeReportingJobConfiguration(
    CloudPolicyClient* client,
    const std::string& server_url,
    bool include_device_info,
    UploadCompleteCallback callback)
    : ReportingJobConfigurationBase(TYPE_UPLOAD_REAL_TIME_REPORT,
                                    client->GetURLLoaderFactory(),
                                    DMAuth::FromDMToken(client->dm_token()),
                                    server_url,
                                    std::move(callback)) {
  InitializePayloadInternal(client, include_device_info);
}

RealtimeReportingJobConfiguration::~RealtimeReportingJobConfiguration() =
    default;

bool RealtimeReportingJobConfiguration::AddReport(base::Value::Dict report) {
  base::Value::Dict* context = report.FindDict(kContextKey);
  base::Value::List* events = report.FindList(kEventListKey);
  if (!context || !events) {
    return false;
  }

  // Overwrite internal context. |context_| will be merged with |payload_| in
  // |GetPayload|.
  if (context_.has_value()) {
    context_->Merge(std::move(*context));
  } else {
    context_ = std::move(*context);
  }

  // Append event_list to the payload.
  base::Value::List* to = payload_.FindList(kEventListKey);
  for (auto& event : *events) {
    to->Append(std::move(event));
  }
  return true;
}

void RealtimeReportingJobConfiguration::InitializePayloadInternal(
    CloudPolicyClient* client,
    bool include_device_info) {
  if (include_device_info) {
    InitializePayloadWithDeviceInfo(client->dm_token(), client->client_id());
  } else {
    InitializePayloadWithoutDeviceInfo();
  }

  payload_.Set(kEventListKey, base::Value::List());
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
    auto* events = payload_.FindList(kEventListKey);
    // Only keep the elements that temporarily failed their uploads.
    events->EraseIf([&failedIds](const base::Value& entry) {
      auto* id = entry.GetDict().FindString(kEventIdKey);
      return id && failedIds.find(*id) == failedIds.end();
    });
  }
}

bool RealtimeReportingJobConfiguration::ShouldRecordUma() const {
  return false;
}

std::string RealtimeReportingJobConfiguration::GetUmaString() const {
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::set<std::string> RealtimeReportingJobConfiguration::GetFailedUploadIds(
    const std::string& response_body) const {
  std::set<std::string> failedIds;
  std::optional<base::Value> response = base::JSONReader::Read(response_body);
  if (!response || !response->is_dict()) {
    return failedIds;
  }
  base::Value::List* failedUploads =
      response->GetDict().FindList(kFailedUploadsKey);
  if (failedUploads) {
    for (const auto& failedUpload : *failedUploads) {
      if (failedUpload.is_dict()) {
        const auto* const id = failedUpload.GetDict().FindString(kEventIdKey);
        if (id) {
          failedIds.insert(*id);
        }
      }
    }
  }
  return failedIds;
}

}  // namespace policy
