// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"

namespace policy {

namespace {

// The URL for the device management server.
const char kDefaultDeviceManagementServerUrl[] =
    "https://m.google.com/devicemanagement/data/api";

const char kDefaultEncryptedReportingServerUrl[] =
    "https://chromereporting-pa.googleapis.com/v1/record";

// The URL for the realtime reporting server.
const char kDefaultRealtimeReportingServerUrl[] =
    "https://chromereporting-pa.googleapis.com/v1/events";

// The URL suffix for the File Storage Server endpoint in DMServer. File Storage
// Server receives the requests on this URL.
const char kFileStorageServerUploadUrlSuffixForDMServer[] = "/upload";

}  // namespace

BrowserPolicyConnector::BrowserPolicyConnector(
    const HandlerListFactory& handler_list_factory)
    : BrowserPolicyConnectorBase(handler_list_factory) {
}

BrowserPolicyConnector::~BrowserPolicyConnector() = default;

void BrowserPolicyConnector::InitInternal(
    PrefService* local_state,
    std::unique_ptr<DeviceManagementService> device_management_service) {
  device_management_service_ = std::move(device_management_service);

  policy_statistics_collector_ =
      std::make_unique<policy::PolicyStatisticsCollector>(
          base::BindRepeating(&GetChromePolicyDetails), GetChromeSchema(),
          GetPolicyService(), local_state,
          base::SingleThreadTaskRunner::GetCurrentDefault());
  policy_statistics_collector_->Initialize();
}

void BrowserPolicyConnector::Shutdown() {
  BrowserPolicyConnectorBase::Shutdown();
  device_management_service_.reset();
  policy_statistics_collector_.reset();
}

void BrowserPolicyConnector::ScheduleServiceInitialization(
    int64_t delay_milliseconds) {
  // Skip device initialization if the BrowserPolicyConnector was never
  // initialized (unit tests).
  if (device_management_service_)
    device_management_service_->ScheduleInitialization(delay_milliseconds);
  else
    CHECK_IS_TEST();
}

bool BrowserPolicyConnector::ProviderHasPolicies(
    const ConfigurationPolicyProvider* provider) const {
  if (!provider || !provider->is_active()) {
    return false;
  }
  for (const auto& pair : provider->policies()) {
    if (!pair.second.empty())
      return true;
  }
  return false;
}

std::string BrowserPolicyConnector::GetDeviceManagementUrl() const {
  return GetUrlOverride(switches::kDeviceManagementUrl,
                        kDefaultDeviceManagementServerUrl);
}

std::string BrowserPolicyConnector::GetRealtimeReportingUrl() const {
  return GetUrlOverride(switches::kRealtimeReportingUrl,
                        kDefaultRealtimeReportingServerUrl);
}

std::string BrowserPolicyConnector::GetEncryptedReportingUrl() const {
  return GetUrlOverride(switches::kEncryptedReportingUrl,
                        kDefaultEncryptedReportingServerUrl);
}

std::string BrowserPolicyConnector::GetFileStorageServerUploadUrl() const {
  return GetUrlOverride(
      switches::kFileStorageServerUploadUrl,
      // The default URL for File Storage Server upload endpoint is
      // extension of the DMServer URL.
      base::StrCat({GetDeviceManagementUrl(),
                    kFileStorageServerUploadUrlSuffixForDMServer}));
}

std::string BrowserPolicyConnector::GetUrlOverride(
    const char* flag,
    std::string_view default_value) const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(flag)) {
    if (IsCommandLineSwitchSupported())
      return command_line->GetSwitchValueASCII(flag);
    else
      LOG(WARNING) << flag << " not supported on this channel";
  }
  return std::string(default_value);
}

// static
void BrowserPolicyConnector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      policy_prefs::kUserPolicyRefreshRate,
      CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
  registry->RegisterBooleanPref(
      policy_prefs::kCloudManagementEnrollmentMandatory, false);
}

}  // namespace policy
