// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_CLIENT_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_CLIENT_BASE_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {
class DeviceManagementService;
}

namespace enterprise_connectors {

// The base class of an event router that observes Safe Browsing events and
// notifies listeners. The router also uploads events to the chrome reporting
// server side API.
class RealtimeReportingClientBase : public KeyedService,
                                    public policy::CloudPolicyClient::Observer {
 public:
  static const char kKeyProfileIdentifier[];
  static const char kKeyProfileUserName[];

  RealtimeReportingClientBase(
      policy::DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  RealtimeReportingClientBase(const RealtimeReportingClientBase&) = delete;
  RealtimeReportingClientBase& operator=(const RealtimeReportingClientBase&) =
      delete;

  ~RealtimeReportingClientBase() override;

  // Returns true if enterprise real-time reporting should be initialized base
  // on feature flag. The default value is true. This function is public
  // so that it can called in tests.
  virtual bool ShouldInitRealtimeReportingClient();

 protected:
  // Sub-method called by InitRealtimeReportingClient() to make appropriate
  // verifications and initialize the profile reporting client. Returns a policy
  // client description and a client, which can be nullptr if it can't be
  // initialized.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  virtual std::pair<std::string, policy::CloudPolicyClient*>
  InitProfileReportingClient(const std::string& dm_token) = 0;
#endif

  // Returns the browser client id required for initializing browser reporting
  // client.
  virtual std::string GetBrowserClientId() = 0;

  // Sub-methods called by ReportEventWithTimestamp() to provide profile related
  // information.
  virtual std::string GetProfileIdentifier() = 0;
  virtual std::string GetProfileUserName() = 0;

  // Sub-method called by ReportEventWithTimestamp() to collect device signals
  // on Windows/Mac/Linux platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  virtual void MaybeCollectDeviceSignals(base::Value::Dict event,
                                         policy::CloudPolicyClient* client,
                                         std::string name,
                                         const ReportingSettings& settings,
                                         base::Time time) = 0;
#endif

  // Returns whether device info should be reported for browser or profile.
  virtual bool ShouldIncludeDeviceInfo() = 0;

  // Callback used with UploadSecurityEventReport() to upload events to the
  // reporting server.
  virtual void UploadCallback(
      base::Value::Dict event_wrapper,
      bool per_profile,
      policy::CloudPolicyClient* client,
      EnterpriseReportingEventType eventType,
      policy::CloudPolicyClient::Result upload_result) = 0;

  // Returns a dictionary of information added to reporting events,
  // corresponding to the Device, Browser and Profile protos defined in
  // google3/google/internal/chrome/reporting/v1/chromereporting.proto.
  virtual base::Value::Dict GetContext() = 0;

 private:
  // Initialize a real-time report client if needed.  This client is used only
  // if real-time reporting is enabled, the machine is properly reigistered
  // with CBCM and the appropriate policies are enabled.
  void InitRealtimeReportingClient(const ReportingSettings& settings);

  // Helper function that uploads security events, parametrized with the time.
  void ReportEventWithTimestamp(const std::string& name,
                                const ReportingSettings& settings,
                                base::Value::Dict event,
                                const base::Time& time,
                                bool include_profile_user_name);

  // Sub-method called by InitRealtimeReportingClient to make appropriate
  // verifications and initialize the browser reporting client. Returns a policy
  // client description and a client, which can be nullptr if it can't be
  // initialized.
  std::pair<std::string, policy::CloudPolicyClient*> InitBrowserReportingClient(
      const std::string& dm_token);

  // Handle the availability of a cloud policy client.
  void OnCloudPolicyClientAvailable(const std::string& policy_client_desc,
                                    policy::CloudPolicyClient* client);

  // Prepares information required by
  // CloudPolicyClient::UploadSecurityEventReport() and calls it.
  void UploadSecurityEventReport(base::Value::Dict event,
                                 policy::CloudPolicyClient* client,
                                 std::string name,
                                 const ReportingSettings& settings,
                                 base::Time time);

  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_ =
      nullptr;

  // The cloud policy clients used to upload browser events and profile events
  // to the cloud. These clients are never used to fetch policies. These
  // pointers are not owned by the class.
  raw_ptr<policy::CloudPolicyClient, DanglingUntriaged> browser_client_ =
      nullptr;
  raw_ptr<policy::CloudPolicyClient, DanglingUntriaged> profile_client_ =
      nullptr;

  // The private clients are used on platforms where we cannot just get a
  // client and we create our own (used through the above client pointers).
  std::unique_ptr<policy::CloudPolicyClient> browser_private_client_;
  std::unique_ptr<policy::CloudPolicyClient> profile_private_client_;

  // When a request is rejected for a given DM token, wait 24 hours before
  // trying again for this specific DM Token.
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      rejected_dm_token_timers_;

  raw_ptr<policy::DeviceManagementService> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<RealtimeReportingClientBase> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_CLIENT_BASE_H_
