// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_SERVICE_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_SERVICE_BASE_H_

#include <optional>

#include "base/types/expected.h"
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
  enum class NoDMTokenForRealTimeUrlCheckReason {
    // Connectors are not enabled, such as for incognito mode.
    kConnectorsDisabled = 0,
    // Connectors are enabled, but the `kEnterpriseRealTimeUrlCheckMode` policy
    // is disabled.
    kPolicyDisabled = 1,
    // Connectors and the `kEnterpriseRealTimeUrlCheckMode` policy are enabled,
    // but there is still no token found.
    kNoDmToken = 2,
    kMaxValue = kNoDmToken,
  };

  explicit ConnectorsServiceBase(
      std::unique_ptr<ConnectorsManagerBase> manager);
  ConnectorsServiceBase(ConnectorsServiceBase&&);
  ConnectorsServiceBase& operator=(ConnectorsServiceBase&&);
  virtual ~ConnectorsServiceBase();

  // DM token accessor function for real-time URL checks. Returns a profile or
  // browser DM token depending on the policy scope. If there is no token to
  // use, returns the reason why.
  base::expected<std::string, NoDMTokenForRealTimeUrlCheckReason>
  GetDMTokenForRealTimeUrlCheck() const;

  // Returns the value to used by the enterprise real-time URL check Connector
  // if it is set and if the scope it's set at has a valid browser-profile
  // affiliation.
  EnterpriseRealTimeUrlCheckMode GetAppliedRealTimeUrlCheck() const;

  // Returns the policy scope of enterprise real-time URL check
  std::optional<policy::PolicyScope> GetRealtimeUrlCheckScope() const;

  // Returns whether the Connectors are enabled.
  virtual bool IsConnectorEnabled(AnalysisConnector connector) const;

  // Returns true if the admin has opted into custom message, learn more URL or
  // letting the user provide bypass justifications in an input dialog.
  bool HasExtraUiToDisplay(AnalysisConnector connector, const std::string& tag);

  bool DelayUntilVerdict(AnalysisConnector connector);

  // Returns true if the admin enabled Bypass Justification.
  bool GetBypassJustificationRequired(AnalysisConnector connector,
                                      const std::string& tag);

  std::vector<std::string> GetReportingServiceProviderNames();

  std::vector<const AnalysisConfig*> GetAnalysisServiceConfigs(
      AnalysisConnector connector);

  std::vector<std::string> GetAnalysisServiceProviderNames(
      AnalysisConnector connector);

  virtual std::optional<ReportingSettings> GetReportingSettings();

  virtual std::optional<std::string> GetBrowserDmToken() const = 0;

  // Gets custom message if set by the admin.
  std::optional<std::u16string> GetCustomMessage(AnalysisConnector connector,
                                                 const std::string& tag);

  // Gets custom learn more URL if provided by the admin.
  std::optional<GURL> GetLearnMoreUrl(AnalysisConnector connector,
                                      const std::string& tag);

  // Obtain a ClientMetadata instance corresponding to the current
  // OnSecurityEvent policy value.  `is_cloud` is true when using a cloud-
  // based service provider and false when using a local service provider.
  virtual std::unique_ptr<ClientMetadata> BuildClientMetadata(
      bool is_cloud) = 0;

  // Observe if reporting policies have changed to include telemetry event.
  void ObserveTelemetryReporting(base::RepeatingCallback<void()> callback);

#if !BUILDFLAG(IS_CHROMEOS)
  std::optional<std::string> GetProfileDmToken() const;
#endif

  // Testing functions.
  ConnectorsManagerBase* ConnectorsManagerBaseForTesting();

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

  // Returns a `policy::CloudPolicyManager` corresponding to a managed user, if
  // one exists.
  virtual policy::CloudPolicyManager* GetManagedUserCloudPolicyManager()
      const = 0;

  void PopulateBrowserMetadata(bool include_device_info,
                               ClientMetadata::Browser* browser_proto);
  void PopulateDeviceMetadata(const std::string& client_id,
                              ClientMetadata::Device* device_proto);

  std::unique_ptr<ConnectorsManagerBase> connectors_manager_base_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_SERVICE_BASE_H_
