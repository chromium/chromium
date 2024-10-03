// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions_internal_overloads.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

namespace {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    const ReportingSettings& settings) {
  // If the corresponding client is already initialized, do nothing.
  if ((settings.per_profile &&
       IsClientValid(settings.dm_token, profile_client_)) ||
      (!settings.per_profile &&
       IsClientValid(settings.dm_token, browser_client_))) {
    DVLOG(2) << "Safe browsing real-time event reporting already initialized.";
    return;
  }

  if (!ShouldInitRealtimeReportingClient()) {
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::pair<std::string, policy::CloudPolicyClient*> desc_and_client =
      InitBrowserReportingClient(settings.dm_token);
#else
  std::pair<std::string, policy::CloudPolicyClient*> desc_and_client =
      settings.per_profile ? InitProfileReportingClient(settings.dm_token)
                           : InitBrowserReportingClient(settings.dm_token);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  VLOG(1) << "Ready for safe browsing real-time event reporting.";
}

void RealtimeReportingClientBase::ReportEventWithTimestamp(
    const std::string& name,
    const ReportingSettings& settings,
    base::Value::Dict event,
    const base::Time& time,
    bool include_profile_user_name) {
  if (rejected_dm_token_timers_.contains(settings.dm_token)) {
    return;
  }

#ifndef NDEBUG
  // Make sure the event is included in the kAllReportingEvents array.
  bool found = false;
  for (const char* event_name : kAllReportingEvents) {
    if (event_name == name) {
      found = true;
      break;
    }
  }
  DCHECK(found);
#endif

  // Make sure real-time reporting is initialized.
  InitRealtimeReportingClient(settings);
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
  MaybeCollectDeviceSignals(std::move(event), client, name, settings, time);
#endif
  UploadSecurityEventReport(std::move(event), client, name, settings, time);
}

void RealtimeReportingClientBase::UploadSecurityEventReport(
    base::Value::Dict event,
    policy::CloudPolicyClient* client,
    std::string name,
    const ReportingSettings& settings,
    base::Time time) {
  base::Value::Dict event_wrapper =
      base::Value::Dict()
          .Set("time", base::TimeFormatAsIso8601(time))
          .Set(name, std::move(event));

  VLOG(1) << "enterprise.connectors: security event: "
          << event_wrapper.DebugString();

  base::Value::Dict report =
      policy::RealtimeReportingJobConfiguration::BuildReport(
          base::Value::List().Append(std::move(event_wrapper)), GetContext());

  auto upload_callback = base::BindOnce(
      &RealtimeReportingClientBase::UploadCallback,
      weak_ptr_factory_.GetWeakPtr(), report.Clone(), settings.per_profile,
      client, enterprise_connectors::GetUmaEnumFromEventName(name));

  client->UploadSecurityEventReport(
      ShouldIncludeDeviceInfo(), std::move(report), std::move(upload_callback));
}

bool RealtimeReportingClientBase::ShouldInitRealtimeReportingClient() {
  return true;
}

}  // namespace enterprise_connectors
