// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "ui/chromeos/devicetype_utils.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

namespace chrome {

namespace {

std::string GetManagedBy(const policy::CloudPolicyManager* manager) {
  if (manager) {
    const enterprise_management::PolicyData* policy =
        manager->core()->store()->policy();
    if (policy && policy->has_managed_by()) {
      return policy->managed_by();
    }
  }
  return std::string();
}

const policy::CloudPolicyManager* GetUserCloudPolicyManager(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return profile->GetUserCloudPolicyManagerChromeOS();
#else
  return profile->GetUserCloudPolicyManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

base::Optional<std::string> GetEnterpriseAccountDomain(Profile* profile) {
  const std::string domain =
      enterprise_util::GetDomainFromEmail(profile->GetProfileUserName());
  // Heuristic for most common consumer Google domains -- these are not managed.
  if (domain.empty() || domain == "gmail.com" || domain == "googlemail.com")
    return base::nullopt;
  return domain;
}

}  // namespace

bool ShouldDisplayManagedUi(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Don't show the UI in demo mode.
  if (chromeos::DemoSession::IsDeviceInDemoMode())
    return false;

  // Don't show the UI for Unicorn accounts.
  if (profile->IsSupervised())
    return false;
#endif

  return enterprise_util::HasBrowserPoliciesApplied(profile);
}

std::u16string GetManagedUiMenuItemLabel(Profile* profile) {
  base::Optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);

  int string_id = IDS_MANAGED;
  std::vector<std::u16string> replacements;
  if (account_manager) {
    string_id = IDS_MANAGED_BY;
    replacements.push_back(base::UTF8ToUTF16(*account_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

std::u16string GetManagedUiWebUILabel(Profile* profile) {
  base::Optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);

  int string_id = IDS_MANAGED_WITH_HYPERLINK;
  std::vector<std::u16string> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  if (account_manager) {
    string_id = IDS_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(*account_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::u16string GetDeviceManagedUiWebUILabel() {
  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;
  std::vector<std::u16string> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  replacements.push_back(ui::GetChromeOSDeviceName());

  const base::Optional<std::string> device_manager = GetDeviceManagerIdentity();
  if (device_manager && !device_manager->empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(*device_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#endif

base::Optional<std::string> GetDeviceManagerIdentity() {
  if (!policy::PlatformManagementService::GetInstance().IsManaged())
    return base::nullopt;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsActiveDirectoryManaged()
             ? connector->GetRealm()
             : connector->GetEnterpriseDomainManager();
#else
  return GetManagedBy(g_browser_process->browser_policy_connector()
                          ->machine_level_user_cloud_policy_manager());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

base::Optional<std::string> GetAccountManagerIdentity(Profile* profile) {
  // TODO(crbug.com/1188594): Replace the check with
  // !policy::BrowserManagementService(profile).IsManaged() once this bug is
  // fixed (it still needs a lot of test fixture changes).
  if (!profile->GetProfilePolicyConnector()->IsManaged())
    return base::nullopt;

  const std::string managed_by =
      GetManagedBy(GetUserCloudPolicyManager(profile));
  if (!managed_by.empty())
    return managed_by;

  return GetEnterpriseAccountDomain(profile);
}

}  // namespace chrome
