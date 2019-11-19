// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector_ios.h"

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_loader_ios.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

class DeviceManagementServiceConfiguration
    : public DeviceManagementService::Configuration {
 public:
  explicit DeviceManagementServiceConfiguration(const std::string& user_agent)
      : user_agent_(user_agent) {}

  ~DeviceManagementServiceConfiguration() override {}

  std::string GetDMServerUrl() override {
    return BrowserPolicyConnector::GetDeviceManagementUrl();
  }

  std::string GetAgentParameter() override { return user_agent_; }

  std::string GetPlatformParameter() override {
    std::string os_name = base::SysInfo::OperatingSystemName();
    std::string os_hardware = base::SysInfo::OperatingSystemArchitecture();
    std::string os_version("-");
    int32_t os_major_version = 0;
    int32_t os_minor_version = 0;
    int32_t os_bugfix_version = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                                 &os_minor_version,
                                                 &os_bugfix_version);
    os_version = base::StringPrintf("%d.%d.%d",
                                    os_major_version,
                                    os_minor_version,
                                    os_bugfix_version);
    return base::StringPrintf(
        "%s|%s|%s", os_name.c_str(), os_hardware.c_str(), os_version.c_str());
  }

  std::string GetReportingServerUrl() override {
    return BrowserPolicyConnector::GetRealtimeReportingUrl();
  }

 private:
  std::string user_agent_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementServiceConfiguration);
};

}  // namespace

BrowserPolicyConnectorIOS::BrowserPolicyConnectorIOS(
    const HandlerListFactory& handler_list_factory,
    const std::string& user_agent,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : BrowserPolicyConnector(handler_list_factory),
      user_agent_(user_agent) {
  std::unique_ptr<AsyncPolicyLoader> loader(
      new PolicyLoaderIOS(background_task_runner));
  std::unique_ptr<ConfigurationPolicyProvider> provider(
      new AsyncPolicyProvider(GetSchemaRegistry(), std::move(loader)));
  SetPlatformPolicyProvider(std::move(provider));
}

BrowserPolicyConnectorIOS::~BrowserPolicyConnectorIOS() {}

void BrowserPolicyConnectorIOS::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  std::unique_ptr<DeviceManagementService::Configuration> configuration(
      new DeviceManagementServiceConfiguration(user_agent_));
  std::unique_ptr<DeviceManagementService> device_management_service(
      new DeviceManagementService(std::move(configuration)));

  // Delay initialization of the cloud policy requests by 5 seconds.
  const int64_t kServiceInitializationStartupDelay = 5000;
  device_management_service->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  InitInternal(local_state, std::move(device_management_service));
}

}  // namespace policy
