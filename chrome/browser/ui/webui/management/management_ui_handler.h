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
#include "chrome/common/url_constants.h"
#include "components/enterprise/browser/promotion/promotion_eligibility_checker.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {
class PolicyService;
}  // namespace policy

namespace enterprise_management {
class GetUserEligiblePromotionsResponse;
}  // namespace enterprise_management

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace device_signals {
class UserPermissionService;
}  // namespace device_signals
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Interface for observing promotion eligibility fetching events.
class ManagementPromotionObserver : public base::CheckedObserver {
 public:
  virtual void OnPromotionEligibilityFetched(
      const std::string& callback_id,
      enterprise_management::GetUserEligiblePromotionsResponse response) = 0;
};

class Profile;

// The JavaScript message handler for the chrome://management page.
class ManagementUIHandler : public content::WebUIMessageHandler,
                            public extensions::ExtensionRegistryObserver,
                            public policy::PolicyService::Observer {
 public:
  explicit ManagementUIHandler(Profile* profile);

  ManagementUIHandler(const ManagementUIHandler&) = delete;
  ManagementUIHandler& operator=(const ManagementUIHandler&) = delete;

  ~ManagementUIHandler() override;

  static std::unique_ptr<ManagementUIHandler> Create(Profile* profile);

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void SetAccountManagedForTesting(bool managed) { account_managed_ = managed; }
#if !BUILDFLAG(IS_CHROMEOS)
  void SetBrowserManagedForTesting(bool managed) { browser_managed_ = managed; }
#endif

  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Observer management for OnPromotionEligibilityFetched.
  void AddManagementPromotionObserver(ManagementPromotionObserver* observer);
  void RemoveManagementPromotionObserver(ManagementPromotionObserver* observer);

  bool HasPromotionBeenChecked() const {
    return has_checked_promotion_eligibility_;
  }

 protected:
  void AddReportingInfo(base::Value::List* report_sources, bool is_browser);

  virtual base::Value::Dict GetContextualManagedData(Profile* profile);
  base::Value::Dict GetThreatProtectionInfo(Profile* profile);
  base::Value::List GetManagedWebsitesInfo(Profile* profile) const;
  base::Value::List GetApplicationsInfo(Profile* profile) const;
  virtual policy::PolicyService* GetPolicyService();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  virtual device_signals::UserPermissionService* GetUserPermissionService();
  base::Value::Dict GetDeviceSignalGrantedMessage();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  bool account_managed() const { return account_managed_; }
  virtual bool managed() const;

  virtual void RegisterPrefChange(PrefChangeRegistrar& pref_registrar);
  virtual void UpdateManagedState();

  bool UpdateAccountManagedState(Profile* profile);
#if !BUILDFLAG(IS_CHROMEOS)
  bool UpdateBrowserManagedState();
#endif

  std::string GetAccountManager(Profile* profile) const;

  bool IsProfileManaged(Profile* profile) const;

  void NotifyThreatProtectionInfoUpdated();

 private:
  void HandleGetExtensions(const base::Value::List& args);
  void HandleGetContextualManagedData(const base::Value::List& args);
  void HandleGetThreatProtectionInfo(const base::Value::List& args);
  void HandleGetManagedWebsites(const base::Value::List& args);
  void HandleGetApplications(const base::Value::List& args);
  void HandleInitBrowserReportingInfo(const base::Value::List& args);
  void HandleInitProfileReportingInfo(const base::Value::List& args);
  void HandleShouldShowPromotion(const base::Value::List& args);
  void HandleSetBannerDismissed(const base::Value::List& args);
  void HandleRecordBannerRedirected(const base::Value::List& args);
  void OnPromotionEligibilityFetched(
      const std::string& callback_id,
      enterprise_management::GetUserEligiblePromotionsResponse response);
  std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
      promotion_eligibility_checker_;

  void AsyncUpdateLogo();

  void NotifyBrowserReportingInfoUpdated();
  void NotifyProfileReportingInfoUpdated();

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // policy::PolicyService::Observer
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  void AddObservers();
  void RemoveObservers();

  bool account_managed_ = false;
  bool browser_managed_ = false;
  // To avoid double-removing the observers, which would cause a DCHECK()
  // failure.
  bool has_observers_ = false;
  std::string web_ui_data_source_name_;

  PrefChangeRegistrar pref_registrar_;

  std::set<extensions::ExtensionId> reporting_extension_ids_;

  // List of observers for promotion eligibility.
  base::ObserverList<ManagementPromotionObserver>
      promotion_eligibility_observers_;

  bool has_checked_promotion_eligibility_ = false;

  base::WeakPtrFactory<ManagementUIHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_HANDLER_H_
