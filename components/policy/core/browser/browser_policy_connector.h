// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_
#define COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/policy_export.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class DeviceManagementService;
class PolicyStatisticsCollector;

// The BrowserPolicyConnector keeps some shared components of the policy system.
// This is a basic implementation that gets extended by platform-specific
// subclasses.
class POLICY_EXPORT BrowserPolicyConnector : public BrowserPolicyConnectorBase {
 public:
  ~BrowserPolicyConnector() override;

  // Finalizes the initialization of the connector. This call can be skipped on
  // tests that don't require the full policy system running.
  virtual void Init(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;

  // Checks whether this device is under any kind of enterprise management.
  virtual bool IsEnterpriseManaged() const = 0;

  // Checks whether there are any machine-level policies configured.
  virtual bool HasMachineLevelPolicies() = 0;

  // Cleans up the connector before it can be safely deleted.
  void Shutdown() override;

  // Schedules initialization of the cloud policy backend services, if the
  // services are already constructed.
  void ScheduleServiceInitialization(int64_t delay_milliseconds);

  DeviceManagementService* device_management_service() {
    return device_management_service_.get();
  }

  // Check whether a user is known to be non-enterprise. Domains such as
  // gmail.com and googlemail.com are known to not be managed. Also returns
  // false if the username is empty.
  static bool IsNonEnterpriseUser(const std::string& username);

  // Allows to register domain for tests that is recognized as non-enterprise.
  // Note that |domain| basically needs to live until this method is invoked
  // with a nullptr.
  static void SetNonEnterpriseDomainForTesting(const char* domain);

  // Returns the URL for the device management service endpoint.
  static std::string GetDeviceManagementUrl();

  // Returns the URL for the realtime reporting service endpoint.
  static std::string GetRealtimeReportingUrl();

  // Registers refresh rate prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // Builds an uninitialized BrowserPolicyConnector.
  // Init() should be called to create and start the policy components.
  explicit BrowserPolicyConnector(
      const HandlerListFactory& handler_list_factory);

  // Helper for the public Init() that must be called by subclasses.
  void InitInternal(
      PrefService* local_state,
      std::unique_ptr<DeviceManagementService> device_management_service);

 private:
  std::unique_ptr<PolicyStatisticsCollector> policy_statistics_collector_;

  std::unique_ptr<DeviceManagementService> device_management_service_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnector);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_
