// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_SERVICE_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_SERVICE_BASE_H_

#include <optional>

#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_manager_base.h"
#include "components/policy/core/common/policy_types.h"

class PrefService;

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace enterprise_connectors {

// Abstract class to access Connector policy values and related information.
class ConnectorsServiceBase {
 public:
  // DM token accessor function for real-time URL checks. Returns a profile or
  // browser DM token depending on the policy scope, and std::nullopt if there
  // is no token to use.
  std::optional<std::string> GetDMTokenForRealTimeUrlCheck() const;

  // Returns the value to used by the enterprise real-time URL check Connector
  // if it is set and if the scope it's set at has a valid browser-profile
  // affiliation.
  EnterpriseRealTimeUrlCheckMode GetAppliedRealTimeUrlCheck() const;

  // Returns whether the Connectors are enabled.
  virtual bool IsConnectorEnabled(AnalysisConnector connector) const = 0;
  bool IsConnectorEnabled(ReportingConnector connector) const;

  std::vector<std::string> GetReportingServiceProviderNames(
      ReportingConnector connector);

  virtual std::optional<ReportingSettings> GetReportingSettings(
      ReportingConnector connector);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<std::string> GetProfileDmToken() const;
#endif

 protected:
  struct DmToken {
    DmToken(const std::string& value, policy::PolicyScope scope);
    DmToken(DmToken&&);
    DmToken& operator=(DmToken&&);
    DmToken(const DmToken&);
    DmToken& operator=(const DmToken&);
    ~DmToken();

    // The value of the token to use.
    std::string value;

    // The scope of the token. This is determined by the scope of the Connector
    // policy used to get a DM token.
    policy::PolicyScope scope;
  };

  // Returns the DM token to use with the given `scope_pref`. That pref should
  // contain either `POLICY_SCOPE_MACHINE` or `POLICY_SCOPE_USER`.
  virtual std::optional<DmToken> GetDmToken(const char* scope_pref) const = 0;

  // Returns whether Connectors are enabled at all. This can be false if the
  // profile is incognito
  virtual bool ConnectorsEnabled() const = 0;

  // Returns the `PrefService` that should be used by this class to lookup
  // prefs. Should never return nullptr.
  virtual PrefService* GetPrefs() = 0;
  virtual const PrefService* GetPrefs() const = 0;

  // Returns the `ConnectorsManagerBase` that should be used by this class to
  // return reporting connector related settings. Should never return nullptr.
  virtual ConnectorsManagerBase* GetConnectorsManagerBase() = 0;
  virtual const ConnectorsManagerBase* GetConnectorsManagerBase() const = 0;

  // Returns a `policy::CloudPolicyManager` corresponding to a managed user, if
  // one exists.
  virtual policy::CloudPolicyManager* GetManagedUserCloudPolicyManager()
      const = 0;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_SERVICE_BASE_H_
