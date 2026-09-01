// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"

#include <optional>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "components/enterprise/common/strings.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_event_mappings.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

const char kBinaryProtobufContentType[] = "application/x-protobuf";

const char RealtimeReportingJobConfiguration::kEventIdKey[] = "eventId";
const char RealtimeReportingJobConfiguration::kUploadedEventsKey[] =
    "uploadedEventIds";
const char RealtimeReportingJobConfiguration::kFailedUploadsKey[] =
    "failedUploads";
const char RealtimeReportingJobConfiguration::kPermanentFailedUploadsKey[] =
    "permanentFailedUploads";


RealtimeReportingJobConfiguration::RealtimeReportingJobConfiguration(
    CloudPolicyClient* client,
    const std::string& server_url,
    bool include_device_info,
    UploadCompleteCallback callback)
    : ReportingJobConfigurationBase(
          TYPE_UPLOAD_REAL_TIME_REPORT,
          client->GetURLLoaderFactory(),
          DMAuth::FromDMToken(client->dm_token()),
          server_url,
          base::BindOnce(&RealtimeReportingJobConfiguration::OnUploadComplete,
                         base::Unretained(this))),
      complete_callback_(std::move(callback)) {
  InitializeUploadRequest(client, include_device_info);
}

void RealtimeReportingJobConfiguration::OnUploadComplete(
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int response_code,
    std::optional<base::DictValue> response) {
  if (complete_callback_) {
    std::move(complete_callback_)
        .Run(job, status, response_code, std::move(response), upload_request_);
  }
}

RealtimeReportingJobConfiguration::~RealtimeReportingJobConfiguration() =
    default;

std::string RealtimeReportingJobConfiguration::GetPayload() {
  std::string payload;
  upload_request_.SerializeToString(&payload);
  const auto& event_case = upload_request_.events(0).event_case();
  const std::string metric_name =
      enterprise_connectors::GetPayloadSizeUmaMetricName(event_case);
  base::UmaHistogramCounts100000(
      enterprise_connectors::kAllUploadSizeUmaMetricName, payload.size());
  if (!metric_name.empty()) {
    base::UmaHistogramCounts100000(metric_name, payload.size());
  }
  return payload;
}

std::string RealtimeReportingJobConfiguration::GetContentType() {
  return kBinaryProtobufContentType;
}

bool RealtimeReportingJobConfiguration::AddRequest(
    ::chrome::cros::reporting::proto::UploadEventsRequest request) {
  upload_request_.MergeFrom(request);
  return true;
}


void RealtimeReportingJobConfiguration::InitializeUploadRequest(
    CloudPolicyClient* client,
    bool include_device_info) {
  AddParameter("key", google_apis::GetAPIKey());
  if (include_device_info) {
    upload_request_.mutable_device()->MergeFrom(
        DeviceDictionaryBuilder::BuildDeviceProto(client->dm_token(),
                                                  client->client_id()));
  }
  upload_request_.mutable_browser()->MergeFrom(
      BrowserDictionaryBuilder::BuildBrowserProto(include_device_info));
}


DeviceManagementService::Job::RetryMethod
RealtimeReportingJobConfiguration::ShouldRetryInternal(
    int response_code,
    const std::string& response_body) {
  DeviceManagementService::Job::RetryMethod retry_method =
      DeviceManagementService::Job::NO_RETRY;
  const auto failed_ids = GetFailedUploadIds(response_body);
  if (!failed_ids.empty()) {
    retry_method = DeviceManagementService::Job::RETRY_WITH_DELAY;
  }
  return retry_method;
}

void RealtimeReportingJobConfiguration::OnBeforeRetryInternal(
    int response_code,
    const std::string& response_body) {
  const auto& failed_ids = GetFailedUploadIds(response_body);
  if (!failed_ids.empty()) {
    auto* events = upload_request_.mutable_events();
    // Events that did not temporarily fail.
    auto events_to_remove = std::remove_if(
        events->begin(), events->end(), [&failed_ids](const auto& event) {
          return failed_ids.find(event.event_id()) == failed_ids.end();
        });
    if (events_to_remove != events->end()) {
      events->erase(events_to_remove, events->end());
    }
  }
}

bool RealtimeReportingJobConfiguration::ShouldRecordUma() const {
  return false;
}

std::string RealtimeReportingJobConfiguration::GetUmaString() const {
  NOTREACHED();
}

std::set<std::string> RealtimeReportingJobConfiguration::GetFailedUploadIds(
    const std::string& response_body) const {
  std::set<std::string> failed_ids;
  std::optional<base::Value> response = base::JSONReader::Read(
      response_body, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!response || !response->is_dict()) {
    return failed_ids;
  }
  const base::ListValue* failed_uploads =
      response->GetDict().FindList(kFailedUploadsKey);
  if (!failed_uploads) {
    return failed_ids;
  }
  for (const auto& failed_upload : *failed_uploads) {
    if (!failed_upload.is_dict()) {
      continue;
    }
    const std::string* id = failed_upload.GetDict().FindString(kEventIdKey);
    if (id) {
      failed_ids.insert(*id);
    }
  }
  return failed_ids;
}

}  // namespace policy
