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
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
// Constants defining the IDs for the localized strings sent to the page as
// load time data.
extern const char kManagementLogUploadEnabled[];
extern const char kManagementReportActivityTimes[];
extern const char kManagementReportHardwareStatus[];
extern const char kManagementReportNetworkInterfaces[];
extern const char kManagementReportUsers[];
extern const char kManagementPrinting[];
extern const char kManagementCrostini[];
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
extern const char kManagementEnterpriseReportingName[];
extern const char kManagementEnterpriseReportingPermissions[];

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
class PolicyService;
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

  static std::string GetAccountDomain(Profile* profile);

  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 protected:
  // Protected for testing.
  static void InitializeInternal(content::WebUI* web_ui,
                                 content::WebUIDataSource* source,
                                 Profile* profile);
  void AddReportingInfo(base::Value* report_sources);

  base::DictionaryValue GetContextualManagedData(Profile* profile);
  base::Value GetThreatProtectionInfo(Profile* profile) const;
  virtual policy::PolicyService* GetPolicyService() const;
  virtual const extensions::Extension* GetEnabledExtension(
      const std::string& extensionId) const;

#if defined(OS_CHROMEOS)
  // Protected for testing.
  virtual const std::string GetDeviceDomain() const;
#endif  // defined(OS_CHROMEOS)
 private:
  void GetManagementStatus(Profile* profile, base::Value* status) const;

#if defined(OS_CHROMEOS)
  void HandleGetDeviceReportingInfo(const base::ListValue* args);
#endif  // defined(OS_CHROMEOS)

  void HandleGetExtensions(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  void HandleGetLocalTrustRootsInfo(const base::ListValue* args);
#endif  // defined(OS_CHROMEOS)

  void HandleGetContextualManagedData(const base::ListValue* args);
  void HandleGetThreatProtectionInfo(const base::ListValue* args);
  void HandleInitBrowserReportingInfo(const base::ListValue* args);

  void AsyncUpdateLogo();

  // BitmapFetcherDelegate
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  void NotifyBrowserReportingInfoUpdated();
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
