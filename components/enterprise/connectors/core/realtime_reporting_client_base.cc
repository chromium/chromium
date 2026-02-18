// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"

#include <algorithm>
#include <ctime>

#include "base/containers/to_value_list.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#include "components/safe_browsing/core/common/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

namespace {
#if BUILDFLAG(IS_CHROMEOS)
const char kPolicyClientDescription[] = "any";
#else
const char kChromeBrowserCloudManagementClientDescription[] =
    "a machine-level user";
#endif
const char kProfilePolicyClientDescription[] = "a profile-level user";

bool IsClientValid(const std::string& dm_token,
                   policy::CloudPolicyClient* client) {
  return client && client->dm_token() == dm_token;
}
}  // namespace

const char RealtimeReportingClientBase::kKeyProfileIdentifier[] =
    "profileIdentifier";
const char RealtimeReportingClientBase::kKeyProfileUserName[] =
    "profileUserName";

RealtimeReportingClientBase::RealtimeReportingClientBase(
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : device_management_service_(device_management_service),
      url_loader_factory_(url_loader_factory) {}

RealtimeReportingClientBase::~RealtimeReportingClientBase() {
  if (browser_client_) {
    browser_client_->RemoveObserver(this);
  }
  if (profile_client_) {
    profile_client_->RemoveObserver(this);
  }
}

void RealtimeReportingClientBase::InitRealtimeReportingClient(
    bool per_profile,
    const std::string& dm_token) {
  // If the corresponding client is already initialized, do nothing.
  if ((per_profile && IsClientValid(dm_token, profile_client_)) ||
      (!per_profile && IsClientValid(dm_token, browser_client_))) {
    DVLOG(2) << "Safe browsing real-time event reporting already initialized.";
    return;
  }

  // |identity_manager_| may be null in tests and in guest profiles. If there
  // is no identity manager then the profile username will be empty.
  if (!identity_manager_) {
    DVLOG(2)
        << "Safe browsing real-time event reporting empty profile username.";
  }

  policy::CloudPolicyClient* client = nullptr;
  std::string policy_client_desc;
#if BUILDFLAG(IS_CHROMEOS)
  std::pair<std::string, policy::CloudPolicyClient*> desc_and_client =
      InitBrowserReportingClient(dm_token);
#else
  std::pair<std::string, policy::CloudPolicyClient*> desc_and_client =
      per_profile ? InitProfileReportingClient(dm_token)
                  : InitBrowserReportingClient(dm_token);
#endif
  if (!desc_and_client.second) {
    return;
  }
  policy_client_desc = std::move(desc_and_client.first);
  client = std::move(desc_and_client.second);

  OnCloudPolicyClientAvailable(policy_client_desc, client);
}

std::pair<std::string, policy::CloudPolicyClient*>
RealtimeReportingClientBase::InitBrowserReportingClient(
    const std::string& dm_token) {
  std::string policy_client_desc;
#if BUILDFLAG(IS_CHROMEOS)
  policy_client_desc = kPolicyClientDescription;
#else
  policy_client_desc = kChromeBrowserCloudManagementClientDescription;
#endif
  if (!device_management_service_) {
    DVLOG(2) << "Safe browsing real-time event requires a device management "
                "service.";
    return {policy_client_desc, nullptr};
  }

  std::string client_id = GetBrowserClientId();
  DCHECK(!client_id.empty());

  // Make sure DeviceManagementService has been initialized.
  device_management_service_->ScheduleInitialization(0);

  browser_private_client_ = std::make_unique<policy::CloudPolicyClient>(
      device_management_service_, url_loader_factory_,
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  policy::CloudPolicyClient* client = browser_private_client_.get();

  if (!client->is_registered()) {
    client->SetupRegistration(
        dm_token, client_id,
        /*user_affiliation_ids=*/std::vector<std::string>());
  }

  return {policy_client_desc, client};
}

policy::CloudPolicyClient* RealtimeReportingClientBase::GetReportingClient(
    const std::string& dm_token,
    bool per_profile) {
  if (rejected_dm_token_timers_.contains(dm_token)) {
    return nullptr;
  }

  InitRealtimeReportingClient(per_profile, dm_token);
  if ((per_profile && !profile_client_) || (!per_profile && !browser_client_)) {
    return nullptr;
  }

  return per_profile ? profile_client_.get() : browser_client_.get();
}

void RealtimeReportingClientBase::OnCloudPolicyClientAvailable(
    const std::string& policy_client_desc,
    policy::CloudPolicyClient* client) {
  if (client == nullptr) {
    LOG(ERROR) << "Could not obtain " << policy_client_desc
               << " for safe browsing real-time event reporting.";
    return;
  }

  if (policy_client_desc == kProfilePolicyClientDescription) {
    DCHECK_NE(profile_client_, client);
    if (profile_client_ == client) {
      return;
    }

    if (profile_client_) {
      profile_client_->RemoveObserver(this);
    }

    profile_client_ = client;
  } else {
    DCHECK_NE(browser_client_, client);
    if (browser_client_ == client) {
      return;
    }

    if (browser_client_) {
      browser_client_->RemoveObserver(this);
    }

    browser_client_ = client;
  }

  client->AddObserver(this);

  DVLOG(1) << "Ready for safe browsing real-time event reporting.";
}

void RealtimeReportingClientBase::ReportEvent(
    ::chrome::cros::reporting::proto::Event event,
    const ReportingSettings& settings) {
  DCHECK(base::FeatureList::IsEnabled(
      policy::kUploadRealtimeReportingEventsUsingProto));
  policy::CloudPolicyClient* client =
      GetReportingClient(settings.dm_token, settings.per_profile);
  if (!client) {
    return;
  }

  // If the timestamp is not set, it's a realtime event so use current time.
  if (!event.has_time()) {
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  MaybeCollectDeviceSignalsAndReportEvent(std::move(event), client, settings);
#else
  // Regardless of collecting device signals or not, upload the security event
  // report.
  UploadSecurityEvent(std::move(event), client, settings);
#endif
}

void RealtimeReportingClientBase::ReportSaasUsageEvent(
    ::chrome::cros::reporting::proto::Event event,
    bool per_profile,
    const std::string& dm_token,
    base::OnceCallback<void(bool)> callback) {
  CHECK(event.has_saas_usage_report_event());
  policy::CloudPolicyClient* client = GetReportingClient(dm_token, per_profile);
  if (!client) {
    std::move(callback).Run(false);
    return;
  }

  if (!event.has_time()) {
    *event.mutable_time() = ToProtoTimestamp(base::Time::Now());
  }

  UploadSaasUsageEvent(std::move(event), client, per_profile, dm_token,
                       std::move(callback));
}

void RealtimeReportingClientBase::UploadSaasUsageEvent(
    ::chrome::cros::reporting::proto::Event event,
    policy::CloudPolicyClient* client,
    bool per_profile,
    const std::string& dm_token,
    base::OnceCallback<void(bool)> callback) {
  ::chrome::cros::reporting::proto::UploadEventsRequest request =
      CreateUploadEventsRequest();
  request.add_events()->Swap(&event);

  auto on_upload_completed = base::BindOnce(
      &RealtimeReportingClientBase::OnSaasUsageEventUploadCompleted,
      AsWeakPtr(), std::move(callback), base::TimeTicks::Now());

  client->UploadSecurityEvent(ShouldIncludeDeviceInfo(per_profile),
                              std::move(request),
                              std::move(on_upload_completed));
}

void RealtimeReportingClientBase::OnSaasUsageEventUploadCompleted(
    base::OnceCallback<void(bool)> callback,
    base::TimeTicks upload_started_at,
    policy::CloudPolicyClient::Result upload_result) {
  auto event_type = enterprise_connectors::GetUmaEnumFromEventCase(
      EventCase::kSaasUsageReportEvent);
  base::UmaHistogramEnumeration(upload_result.IsSuccess()
                                    ? "Enterprise.ReportingEventUploadSuccess"
                                    : "Enterprise.ReportingEventUploadFailure",
                                event_type);
  base::UmaHistogramCustomTimes(
      upload_result.IsSuccess()
          ? GetSuccessfulUploadDurationUmaMetricName(event_type)
          : GetFailedUploadDurationUmaMetricName(event_type),
      base::TimeTicks::Now() - upload_started_at, base::Milliseconds(1),
      base::Minutes(5), 50);

  std::move(callback).Run(upload_result.IsSuccess());
}

void RealtimeReportingClientBase::ReportEventWithTimestampDeprecated(
    const std::string& name,
    const ReportingSettings& settings,
    base::DictValue event,
    const base::Time& time,
    bool include_profile_user_name) {
  // TODO(Bug:394403600) - Replace with a DCHECK once all callers are migrated.
  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    DLOG(WARNING) << "ReportEventWithTimestampDeprecated called when proto "
                     "format enabled for event "
                  << name;
    return;
  }

  if (rejected_dm_token_timers_.contains(settings.dm_token)) {
    return;
  }

#ifndef NDEBUG
  // Make sure the event is included in the kAllReportingEnabledEvents or the
  // kAllReportingOptInEvents array.
  bool found = std::ranges::contains(kAllReportingEnabledEvents, name) ||
               std::ranges::contains(kAllReportingOptInEvents, name);
  DCHECK(found);
#endif

  // Make sure real-time reporting is initialized.
  InitRealtimeReportingClient(settings.per_profile, settings.dm_token);
  if ((settings.per_profile && !profile_client_) ||
      (!settings.per_profile && !browser_client_)) {
    return;
  }

  policy::CloudPolicyClient* client =
      settings.per_profile ? profile_client_.get() : browser_client_.get();
  event.Set(kKeyProfileIdentifier, GetProfileIdentifier());
  if (include_profile_user_name) {
    event.Set(kKeyProfileUserName, GetProfileUserName());
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  MaybeCollectDeviceSignalsAndReportEventDeprecated(std::move(event), client,
                                                    name, settings, time);
#else
  // Regardless of collecting device signals or not, upload the security event
  // report.
  UploadSecurityEventReportDeprecated(std::move(event), client, name, settings,
                                      time);
#endif
}

void RealtimeReportingClientBase::UploadSecurityEvent(
    ::chrome::cros::reporting::proto::Event event,
    policy::CloudPolicyClient* client,
    const ReportingSettings& settings) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetLocalIpAddresses),
      base::BindOnce(&RealtimeReportingClientBase::OnIpAddressesFetched,
                     AsWeakPtr(), std::move(event), client, settings));
  return;
}

void RealtimeReportingClientBase::OnIpAddressesFetched(
    ::chrome::cros::reporting::proto::Event event,
    policy::CloudPolicyClient* client,
    const ReportingSettings& settings,
    std::vector<std::string> ip_addresses) {
  event.mutable_local_ips()->Add(ip_addresses.begin(), ip_addresses.end());
  FinishUploadSecurityEvent(std::move(event), client, settings);
}

void RealtimeReportingClientBase::FinishUploadSecurityEvent(
    ::chrome::cros::reporting::proto::Event event,
    policy::CloudPolicyClient* client,
    const ReportingSettings& settings) {
  MaybeTruncateLongUrls(event);
  auto event_type =
      enterprise_connectors::GetUmaEnumFromEventCase(event.event_case());
  ::chrome::cros::reporting::proto::UploadEventsRequest request =
      CreateUploadEventsRequest();
  request.add_events()->Swap(&event);

  auto upload_callback = base::BindOnce(
      &RealtimeReportingClientBase::UploadCallback, AsWeakPtr(), request,
      settings.per_profile, client, event_type, base::TimeTicks::Now());

  client->UploadSecurityEvent(ShouldIncludeDeviceInfo(settings.per_profile),
                              std::move(request), std::move(upload_callback));
}

void RealtimeReportingClientBase::UploadSecurityEventReportDeprecated(
    base::DictValue event,
    policy::CloudPolicyClient* client,
    std::string name,
    const ReportingSettings& settings,
    base::Time time) {
  base::DictValue event_wrapper =
      base::DictValue()
          .Set("time", base::TimeFormatAsIso8601(time))
          .Set(name, std::move(event));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetLocalIpAddresses),
      base::BindOnce(
          &RealtimeReportingClientBase::OnIpAddressesFetchedDeprecated,
          AsWeakPtr(), std::move(event_wrapper), client, name, settings, time));
  return;
}

void RealtimeReportingClientBase::OnIpAddressesFetchedDeprecated(
    base::DictValue event_wrapper,
    policy::CloudPolicyClient* client,
    std::string name,
    const ReportingSettings& settings,
    base::Time time,
    std::vector<std::string> ip_addresses) {
  event_wrapper.Set("localIps", base::ToValueList(ip_addresses));
  FinishUploadSecurityEventReportDeprecated(std::move(event_wrapper), client,
                                            name, settings);
}

void RealtimeReportingClientBase::FinishUploadSecurityEventReportDeprecated(
    base::DictValue event_wrapper,
    policy::CloudPolicyClient* client,
    std::string name,
    const ReportingSettings& settings) {
  DVLOG(1) << "enterprise.connectors: security event: "
           << event_wrapper.DebugString();

  base::DictValue report =
      policy::RealtimeReportingJobConfiguration::BuildReport(
          base::ListValue().Append(std::move(event_wrapper)), GetContext());

  auto upload_callback =
      base::BindOnce(&RealtimeReportingClientBase::UploadCallbackDeprecated,
                     AsWeakPtr(), report.Clone(), settings.per_profile, client,
                     enterprise_connectors::GetUmaEnumFromEventName(name),
                     base::TimeTicks::Now());

  client->UploadSecurityEventReport(
      ShouldIncludeDeviceInfo(settings.per_profile), std::move(report),
      std::move(upload_callback));
}

const std::string
RealtimeReportingClientBase::GetProfilePolicyClientDescription() {
  return kProfilePolicyClientDescription;
}

}  // namespace enterprise_connectors
