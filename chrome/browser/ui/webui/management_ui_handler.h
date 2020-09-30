// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_

#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
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

#if defined(OS_CHROMEOS)
// Constants defining the IDs for the localized strings sent to the page as
// load time data.
extern const char kManagementLogUploadEnabled[];
extern const char kManagementReportActivityTimes[];
extern const char kManagementReportHardwareStatus[];
extern const char kManagementReportNetworkInterfaces[];
extern const char kManagementReportUsers[];
extern const char kManagementReportCrashReports[];
extern const char kManagementReportAppInfoAndActivity[];
extern const char kManagementPrinting[];
extern const char kManagementCrostini[];
extern const char kManagementCrostiniContainerConfiguration[];
extern const char kManagementReportExtensions[];
extern const char kManagementReportAndroidApplications[];
#endif  // defined(OS_CHROMEOS)

extern const char kCloudReportingExtensionId[];
extern const char kOnPremReportingExtensionStableId[];
extern const char kOnPremReportingExtensionBetaId[];

extern const char kManagementExtensionReportMachineName[];
extern const char kManagementExtensionReportMachineNameAddress[];
extern const char kManagementExtensionReportUsername[];
extern const char kManagementExtensionReportVersion[];
extern const char kManagementExtensionReportExtensionsPlugin[];
extern const char kManagementExtensionReportSafeBrowsingWarnings[];
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
extern const char kManagementOnPageVisitedEvent[];
extern const char kManagementOnPageVisitedVisibleData[];

extern const char kPolicyKeyReportMachineIdData[];
extern const char kPolicyKeyReportUserIdData[];
extern const char kPolicyKeyReportVersionData[];
extern const char kPolicyKeyReportPolicyData[];
extern const char kPolicyKeyReportExtensionsData[];
extern const char kPolicyKeyReportSafeBrowsingData[];
extern const char kPolicyKeyReportSystemTelemetryData[];
extern const char kPolicyKeyReportUserBrowsingData[];

extern const char kReportingTypeDevice[];
extern const char kReportingTypeExtensions[];
extern const char kReportingTypeSecurity[];
extern const char kReportingTypeUser[];
extern const char kReportingTypeUserActivity[];

namespace base {
class ListValue;
}  // namespace base

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {
class DeviceCloudPolicyManagerChromeOS;
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
  ~ManagementUIHandler() override;

  static void Initialize(content::WebUI* web_ui,
                         content::WebUIDataSource* source);

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void SetAccountManagedForTesting(bool managed) { account_managed_ = managed; }
  void SetDeviceManagedForTesting(bool managed) { device_managed_ = managed; }

  // This will return the domain (ie foo.com) that manages |profile|. If
  // unmanaged, an empty string is returned.
  static std::string GetAccountDomain(Profile* profile);

  // This returns the entity that manages this |profile|. For standard dasher
  // domains, this will be a domain name (ie foo.com). For FlexOrgs, this will
  // be the email address of the admin of the FlexOrg (ie user@foo.com). If
  // DMServer does not provide this information, this method defaults to
  // |GetAccountDomain|. If unmanaged, an empty string is returned.
  static std::string GetAccountManager(Profile* profile);

  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 protected:
  // Protected for testing.
  static void InitializeInternal(content::WebUI* web_ui,
                                 content::WebUIDataSource* source,
                                 Profile* profile);
  void AddReportingInfo(base::Value* report_sources);

  base::Value GetContextualManagedData(Profile* profile);
  base::Value GetThreatProtectionInfo(Profile* profile) const;
  virtual policy::PolicyService* GetPolicyService() const;
  virtual const extensions::Extension* GetEnabledExtension(
      const std::string& extensionId) const;

#if defined(OS_CHROMEOS)
  // Protected for testing.
  virtual const std::string GetDeviceDomain() const;
  virtual const policy::DeviceCloudPolicyManagerChromeOS*
  GetDeviceCloudPolicyManager() const;
  void AddDeviceReportingInfo(base::Value* report_sources,
                              const policy::StatusCollector* collector,
                              const policy::SystemLogUploader* uploader,
                              Profile* profile) const;
  // Virtual for testing
  virtual bool IsUpdateRequiredEol() const;
  // Adds device return instructions for a managed user as an update is required
  // as per device policy but the device cannot be updated due to End of Life
  // (Auto Update Expiration).
  void AddUpdateRequiredEolInfo(base::Value* response) const;

  // Adds a boolean which indicates if there's a proxy on the device enforced by
  // the admin. If true, a warning will be added to the transparency panel to
  // inform the user that the admin may be able to see their network traffic.
  void AddProxyServerPrivacyDisclosure(base::Value* response) const;
#endif  // defined(OS_CHROMEOS)
 private:
  void GetManagementStatus(Profile* profile, base::Value* status) const;

#if defined(OS_CHROMEOS)
  void HandleGetDeviceReportingInfo(const base::ListValue* args);
  void HandleGetPluginVmDataCollectionStatus(const base::ListValue* args);
  void HandleGetLocalTrustRootsInfo(const base::ListValue* args);
#endif  // defined(OS_CHROMEOS)

  void HandleGetExtensions(const base::ListValue* args);
  void HandleGetContextualManagedData(const base::ListValue* args);
  void HandleGetThreatProtectionInfo(const base::ListValue* args);
  void HandleInitBrowserReportingInfo(const base::ListValue* args);

  void AsyncUpdateLogo();

  // BitmapFetcherDelegate
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  void NotifyBrowserReportingInfoUpdated();
#if defined(OS_CHROMEOS)
  void NotifyPluginVmDataCollectionUpdated();
#endif  // defined(OS_CHROMEOS)
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

  DISALLOW_COPY_AND_ASSIGN(ManagementUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_
