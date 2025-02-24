// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "components/enterprise/common/strings.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

const char kBinaryProtobufContentType[] = "application/x-protobuf";

BASE_FEATURE(kUploadRealtimeReportingEventsUsingProto,
             "UploadRealtimeReportingEventsUsingProto",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
  if (base::FeatureList::IsEnabled(kUploadRealtimeReportingEventsUsingProto)) {
    InitializeUploadRequest(client, include_device_info);
  } else {
    InitializePayloadInternal(client, include_device_info);
  }
}

RealtimeReportingJobConfiguration::~RealtimeReportingJobConfiguration() =
    default;

std::string RealtimeReportingJobConfiguration::GetPayload() {
  std::string payload;
  std::string metric_name;
  if (base::FeatureList::IsEnabled(kUploadRealtimeReportingEventsUsingProto)) {
    upload_request_.SerializeToString(&payload);
    const auto& event_case = upload_request_.events(0).event_case();
    metric_name =
        enterprise_connectors::GetPayloadSizeUmaMetricName(event_case);
  } else {
    payload = ReportingJobConfigurationBase::GetPayload();

    // When kUploadRealtimeReportingEventsUsingProto is disabled, the payload
    // is serialized from the JSON dictionary in |payload_|, so use this
    // dict to find the event type. |payload| still stores the final serialized
    // string in both cases.
    const auto& dict = payload_.FindList(kEventListKey)->front().GetDict();
    std::set<std::string> all_reporting_events;
    all_reporting_events.insert(
        enterprise_connectors::kAllReportingEnabledEvents.begin(),
        enterprise_connectors::kAllReportingEnabledEvents.end());
    all_reporting_events.insert(
        enterprise_connectors::kAllReportingOptInEvents.begin(),
        enterprise_connectors::kAllReportingOptInEvents.end());
    for (const std::string& event_name : all_reporting_events) {
      if (dict.contains(event_name)) {
        metric_name =
            enterprise_connectors::GetPayloadSizeUmaMetricName(event_name);
        break;
      }
    }
  }
  base::UmaHistogramCounts100000(
      enterprise_connectors::kAllUploadSizeUmaMetricName, payload.size());
  if (!metric_name.empty()) {
    base::UmaHistogramCounts100000(metric_name, payload.size());
  }
  return payload;
}

std::string RealtimeReportingJobConfiguration::GetContentType() {
  if (base::FeatureList::IsEnabled(kUploadRealtimeReportingEventsUsingProto)) {
    return kBinaryProtobufContentType;
  }
  return ReportingJobConfigurationBase::GetContentType();
}

bool RealtimeReportingJobConfiguration::AddRequest(
    ::chrome::cros::reporting::proto::UploadEventsRequest request) {
  DCHECK(
      base::FeatureList::IsEnabled(kUploadRealtimeReportingEventsUsingProto));
  upload_request_.MergeFrom(request);
  return true;
}

bool RealtimeReportingJobConfiguration::AddReportDeprecated(
    base::Value::Dict report) {
  DCHECK(
      !base::FeatureList::IsEnabled(kUploadRealtimeReportingEventsUsingProto));
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
    if (base::FeatureList::IsEnabled(
            kUploadRealtimeReportingEventsUsingProto)) {
      auto* events = upload_request_.mutable_events();
      // Events that did not temporarily fail.
      auto events_to_remove = std::remove_if(
          events->begin(), events->end(), [&failedIds](const auto& event) {
            return failedIds.find(event.event_id()) == failedIds.end();
          });
      if (events_to_remove != events->end()) {
        events->erase(events_to_remove, events->end());
      }
    } else {
      auto* events = payload_.FindList(kEventListKey);
      // Only keep the elements that temporarily failed their uploads.
      events->EraseIf([&failedIds](const base::Value& entry) {
        auto* id = entry.GetDict().FindString(kEventIdKey);
        return id && failedIds.find(*id) == failedIds.end();
      });
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
