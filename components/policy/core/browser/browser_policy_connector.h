// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_
#define COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

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
  BrowserPolicyConnector(const BrowserPolicyConnector&) = delete;
  BrowserPolicyConnector& operator=(const BrowserPolicyConnector&) = delete;
  ~BrowserPolicyConnector() override;

  // Finalizes the initialization of the connector. This call can be skipped on
  // tests that don't require the full policy system running.
  virtual void Init(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;

  // Checks whether this device is under any kind of enterprise management.
  virtual bool IsDeviceEnterpriseManaged() const = 0;

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

  // Returns the URL for the device management service endpoint.
  std::string GetDeviceManagementUrl() const;

  // Returns the URL for the realtime reporting service endpoint.
  std::string GetRealtimeReportingUrl() const;

  // Returns the URL for the encrypted reporting service endpoint.
  std::string GetEncryptedReportingUrl() const;

  // Returns the URL for the File Storage Server endpoint for uploads.
  std::string GetFileStorageServerUploadUrl() const;

  // Registers refresh rate prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns true if the command line switch of policy can be used.
  virtual bool IsCommandLineSwitchSupported() const = 0;

 protected:
  // Builds an uninitialized BrowserPolicyConnector.
  // Init() should be called to create and start the policy components.
  explicit BrowserPolicyConnector(
      const HandlerListFactory& handler_list_factory);

  // Helper for the public Init() that must be called by subclasses.
  void InitInternal(
      PrefService* local_state,
      std::unique_ptr<DeviceManagementService> device_management_service);

  // Returns true if the given |provider| has any registered policies.
  bool ProviderHasPolicies(const ConfigurationPolicyProvider* provider) const;

 private:
  // Helper function to read URL overriding flags. If `flag` isn't set or if the
  // Chrome channel doesn't allowing overriding, `default_value` is returned
  // instead.
  std::string GetUrlOverride(const char* flag,
                             std::string_view default_value) const;

  std::unique_ptr<PolicyStatisticsCollector> policy_statistics_collector_;

  std::unique_ptr<DeviceManagementService> device_management_service_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_H_
