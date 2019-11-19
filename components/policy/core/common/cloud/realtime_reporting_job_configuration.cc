// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

const char RealtimeReportingJobConfiguration::kContextKey[] = "context";
const char RealtimeReportingJobConfiguration::kEventListKey[] = "eventList";

const char RealtimeReportingJobConfiguration::kBrowserIdKey[] =
    "browser.browserId";
const char RealtimeReportingJobConfiguration::kChromeVersionKey[] =
    "browser.chromeVersion";
const char RealtimeReportingJobConfiguration::kClientIdKey[] =
    "device.clientId";
const char RealtimeReportingJobConfiguration::kDmTokenKey[] = "device.dmToken";
const char RealtimeReportingJobConfiguration::kEventsKey[] = "events";
const char RealtimeReportingJobConfiguration::kMachineUserKey[] =
    "browser.machineUser";
const char RealtimeReportingJobConfiguration::kOsVersionKey[] =
    "device.osVersion";

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
    std::unique_ptr<DMAuth> auth_data,
    Callback callback)
    : JobConfigurationBase(TYPE_UPLOAD_REAL_TIME_REPORT,
                           std::move(auth_data),
                           base::nullopt,
                           client->GetURLLoaderFactory()),
      server_url_(client->service()->configuration()->GetReportingServerUrl()),
      payload_(base::Value::Type::DICTIONARY),
      callback_(std::move(callback)) {
  DCHECK(GetAuth().has_dm_token());

  AddParameter("key", google_apis::GetAPIKey());
  InitializePayload(client);
}

RealtimeReportingJobConfiguration::~RealtimeReportingJobConfiguration() {}

bool RealtimeReportingJobConfiguration::AddReport(base::Value report) {
  if (!report.is_dict())
    return false;

  base::Optional<base::Value> context = report.ExtractKey(kContextKey);
  base::Optional<base::Value> event_list = report.ExtractKey(kEventListKey);
  if (!context || !event_list || !event_list->is_list())
    return false;

  // Move context keys to the payload.  It is possible to add multiple reports
  // to the payload in which case the context values are the same.
  payload_.MergeDictionary(&*context);

  // Append event_list to the payload.
  base::Value::ListStorage& to = payload_.FindListKey(kEventsKey)->GetList();
  base::Value::ListStorage& from = event_list->GetList();
  to.insert(to.end(), std::make_move_iterator(from.begin()),
            std::make_move_iterator(from.end()));
  return true;
}

void RealtimeReportingJobConfiguration::InitializePayload(
    CloudPolicyClient* client) {
  base::FilePath browser_id;
  if (base::PathService::Get(base::DIR_EXE, &browser_id))
    payload_.SetStringPath(kBrowserIdKey, browser_id.value());

  payload_.SetStringPath(kDmTokenKey, GetAuth().dm_token());
  payload_.SetStringPath(kClientIdKey, client->client_id());
  payload_.SetStringPath(kMachineUserKey, GetOSUsername());
  payload_.SetStringPath(kChromeVersionKey, version_info::GetVersionNumber());
  payload_.SetStringPath(kOsVersionKey, GetOSVersion());
  payload_.SetPath(kEventsKey, base::Value(base::Value::Type::LIST));
}

std::string RealtimeReportingJobConfiguration::GetPayload() {
  std::string payload_string;
  base::JSONWriter::Write(payload_, &payload_string);
  return payload_string;
}

std::string RealtimeReportingJobConfiguration::GetUmaName() {
  return "Enterprise.RealtimeReportingSuccess." + GetJobTypeAsString(GetType());
}

void RealtimeReportingJobConfiguration::OnURLLoadComplete(
    DeviceManagementService::Job* job,
    int net_error,
    int response_code,
    const std::string& response_body) {
  base::Optional<base::Value> response = base::JSONReader::Read(response_body);

  // Parse the response even if |response_code| is not a success since the
  // response data may contain an error message.
  // Map the net_error/response_code to a DeviceManagementStatus.
  DeviceManagementStatus code;
  if (net_error != net::OK) {
    code = DM_STATUS_REQUEST_FAILED;
  } else {
    switch (response_code) {
      case DeviceManagementService::kSuccess:
        code = DM_STATUS_SUCCESS;
        break;
      case DeviceManagementService::kInvalidArgument:
        code = DM_STATUS_REQUEST_INVALID;
        break;
      case DeviceManagementService::kInvalidAuthCookieOrDMToken:
        code = DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID;
        break;
      case DeviceManagementService::kDeviceManagementNotAllowed:
        code = DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED;
        break;
      default:
        // Handle all unknown 5xx HTTP error codes as temporary and any other
        // unknown error as one that needs more time to recover.
        if (response_code >= 500 && response_code <= 599)
          code = DM_STATUS_TEMPORARY_UNAVAILABLE;
        else
          code = DM_STATUS_HTTP_STATUS_ERROR;
        break;
    }
  }

  base::Value response_value = response ? std::move(*response) : base::Value();
  std::move(callback_).Run(job, code, net_error, response_value);
}

GURL RealtimeReportingJobConfiguration::GetURL(int last_error) {
  return GURL(server_url_);
}

}  // namespace policy
