// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_HANDLER_H_

#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/common/url_constants.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// Constants defining the IDs for the localized strings sent to the page as
// load time data.
extern const char kManagementScreenCaptureEvent[];
extern const char kManagementScreenCaptureData[];
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kManagementLogUploadEnabled[];
extern const char kManagementReportActivityTimes[];
extern const char kManagementReportDeviceAudioStatus[];
extern const char kManagementReportDeviceGraphicsStatus[];
extern const char kManagementReportDevicePeripherals[];
extern const char kManagementReportNetworkData[];
extern const char kManagementReportHardwareData[];
extern const char kManagementReportUsers[];
extern const char kManagementReportCrashReports[];
extern const char kManagementReportAppInfoAndActivity[];
extern const char kManagementReportPrintJobs[];
extern const char kManagementReportDlpEvents[];
extern const char kManagementReportLoginLogout[];
extern const char kManagementReportCRDSessions[];
extern const char kManagementPrinting[];
extern const char kManagementCrostini[];
extern const char kManagementCrostiniContainerConfiguration[];
extern const char kManagementReportExtensions[];
extern const char kManagementReportAndroidApplications[];
extern const char kManagementOnFileTransferEvent[];
extern const char kManagementOnFileTransferVisibleData[];
#endif  // BUILDFLAG(IS_CHROMEOS)

extern const char kOnPremReportingExtensionStableId[];
extern const char kOnPremReportingExtensionBetaId[];

extern const char kManagementExtensionReportMachineName[];
extern const char kManagementExtensionReportMachineNameAddress[];
extern const char kManagementExtensionReportUsername[];
extern const char kManagementExtensionReportVersion[];
extern const char kManagementExtensionReportExtensionsPlugin[];
extern const char kManagementExtensionReportPerfCrash[];
extern const char kManagementExtensionReportUserBrowsingData[];

extern const char kThreatProtectionTitle[];
extern const char kManagementDataLossPreventionName[];
extern const char kManagementDataLossPreventionPermissions[];
extern const char kManagementMalwareScanningName[];
extern const char kManagementMalwareScanningPermissions[];
extern const char kManagementEnterpriseReportingEvent[];
extern const char kManagementEnterpriseReportingVisibleData[];
extern const char kManagementOnFileAttachedEvent[];
extern const char kManagementOnFileAttachedVisibleData[];
extern const char kManagementOnFileDownloadedEvent[];
extern const char kManagementOnFileDownloadedVisibleData[];
extern const char kManagementOnBulkDataEntryEvent[];
extern const char kManagementOnBulkDataEntryVisibleData[];
extern const char kManagementOnPrintEvent[];
extern const char kManagementOnPrintVisibleData[];
extern const char kManagementOnPageVisitedEvent[];
extern const char kManagementOnPageVisitedVisibleData[];

extern const char kPolicyKeyReportMachineIdData[];
extern const char kPolicyKeyReportUserIdData[];
extern const char kPolicyKeyReportVersionData[];
extern const char kPolicyKeyReportPolicyData[];
extern const char kPolicyKeyReportDlpEvents[];
extern const char kPolicyKeyReportExtensionsData[];
extern const char kPolicyKeyReportSystemTelemetryData[];
extern const char kPolicyKeyReportUserBrowsingData[];

extern const char kReportingTypeDevice[];
extern const char kReportingTypeExtensions[];
extern const char kReportingTypeSecurity[];
extern const char kReportingTypeUser[];
extern const char kReportingTypeUserActivity[];

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {
class DeviceCloudPolicyManagerAsh;
class PolicyService;
class StatusCollector;
class SystemLogUploader;
}  // namespace policy

class Profile;

// The JavaScript message handler for the chrome://management page.
class ManagementUIHandler : public content::WebUIMessageHandler,
                            public extensions::ExtensionRegistryObserver,
                            public policy::PolicyService::Observer,
                            public BitmapFetcherDelegate {
 public:
  ManagementUIHandler();

  ManagementUIHandler(const ManagementUIHandler&) = delete;
  ManagementUIHandler& operator=(const ManagementUIHandler&) = delete;

  ~ManagementUIHandler() override;

  static void Initialize(content::WebUI* web_ui,
                         content::WebUIDataSource* source);

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void SetAccountManagedForTesting(bool managed) { account_managed_ = managed; }
  void SetDeviceManagedForTesting(bool managed) { device_managed_ = managed; }

  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns the list of device reporting items for a given profile.
  static base::Value::List GetDeviceReportingInfo(
      const policy::DeviceCloudPolicyManagerAsh* manager,
      Profile* profile);
  static void AddDlpDeviceReportingElementForTesting(
      base::Value::List* report_sources,
      const std::string& message_id);
  static void AddDeviceReportingInfoForTesting(
      base::Value::List* report_sources,
      const policy::StatusCollector* collector,
      const policy::SystemLogUploader* uploader,
      Profile* profile);
#endif

 protected:
  // Protected for testing.
  static void InitializeInternal(content::WebUI* web_ui,
                                 content::WebUIDataSource* source,
                                 Profile* profile);
  void AddReportingInfo(base::Value::List* report_sources);

  base::Value::Dict GetContextualManagedData(Profile* profile);
  base::Value::Dict GetThreatProtectionInfo(Profile* profile);
  base::Value::List GetManagedWebsitesInfo(Profile* profile) const;
  virtual policy::PolicyService* GetPolicyService();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Protected for testing.
  virtual const std::string GetDeviceManager() const;
  virtual const policy::DeviceCloudPolicyManagerAsh*
  GetDeviceCloudPolicyManager() const;
  // Virtual for testing
  virtual bool IsUpdateRequiredEol() const;
  // Adds device return instructions for a managed user as an update is required
  // as per device policy but the device cannot be updated due to End of Life
  // (Auto Update Expiration).
  void AddUpdateRequiredEolInfo(base::Value::Dict* response) const;

  // Adds a boolean which indicates if the network traffic can be monitored by
  // the admin via policy configurations, either via a proxy server or via
  // secure DNS templates with identifiers. If true, a warning will be added to
  // the transparency panel to inform the user that the admin may be able to see
  // their network traffic.
  void AddMonitoredNetworkPrivacyDisclosure(base::Value::Dict* response) const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  void GetManagementStatus(Profile* profile, base::Value::Dict* status) const;

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void HandleGetDeviceReportingInfo(const base::Value::List& args);
  void HandleGetPluginVmDataCollectionStatus(const base::Value::List& args);
  void HandleGetLocalTrustRootsInfo(const base::Value::List& args);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnGotDeviceReportSources(base::Value::List report_sources,
                                bool plugin_vm_data_collection_enabled);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  void HandleGetExtensions(const base::Value::List& args);
  void HandleGetContextualManagedData(const base::Value::List& args);
  void HandleGetThreatProtectionInfo(const base::Value::List& args);
  void HandleGetManagedWebsites(const base::Value::List& args);
  void HandleInitBrowserReportingInfo(const base::Value::List& args);

  void AsyncUpdateLogo();

  // BitmapFetcherDelegate
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  void NotifyBrowserReportingInfoUpdated();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void NotifyPluginVmDataCollectionUpdated();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  void NotifyThreatProtectionInfoUpdated();

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  void UpdateManagedState();

  // policy::PolicyService::Observer
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  void AddObservers();
  void RemoveObservers();

  bool managed_() const { return account_managed_ || device_managed_; }
  bool account_managed_ = false;
  bool device_managed_ = false;
  // To avoid double-removing the observers, which would cause a DCHECK()
  // failure.
  bool has_observers_ = false;
  std::string web_ui_data_source_name_;

  PrefChangeRegistrar pref_registrar_;

  std::set<extensions::ExtensionId> reporting_extension_ids_;
  GURL logo_url_;
  std::string fetched_image_;
  std::unique_ptr<BitmapFetcher> icon_fetcher_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  base::Value::List report_sources_;
  bool plugin_vm_data_collection_enabled_ = false;
  base::WeakPtrFactory<ManagementUIHandler> weak_factory_{this};
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_HANDLER_H_
