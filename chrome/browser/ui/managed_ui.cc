// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/webui/policy_data_utils.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "ui/chromeos/devicetype_utils.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace chrome {

namespace {

const policy::CloudPolicyManager* GetUserCloudPolicyManager(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return profile->GetUserCloudPolicyManagerAsh();
#else
  return profile->GetUserCloudPolicyManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

absl::optional<std::string> GetEnterpriseAccountDomain(Profile* profile) {
  const std::string domain =
      enterprise_util::GetDomainFromEmail(profile->GetProfileUserName());
  // Heuristic for most common consumer Google domains -- these are not managed.
  if (domain.empty() || domain == "gmail.com" || domain == "googlemail.com")
    return absl::nullopt;
  return domain;
}

}  // namespace

bool ShouldDisplayManagedUi(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Don't show the UI in demo mode.
  if (ash::DemoSession::IsDeviceInDemoMode())
    return false;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Don't show the UI for Family Link accounts.
  if (profile->IsChild())
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  return enterprise_util::IsBrowserManaged(profile);
}

#if !BUILDFLAG(IS_ANDROID)
std::u16string GetManagedUiMenuItemLabel(Profile* profile) {
  absl::optional<std::string> account_manager =
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
  absl::optional<std::string> account_manager =
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
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::u16string GetDeviceManagedUiWebUILabel() {
  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;
  std::vector<std::u16string> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  replacements.push_back(ui::GetChromeOSDeviceName());

  const absl::optional<std::string> device_manager = GetDeviceManagerIdentity();
  if (device_manager && !device_manager->empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(*device_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#endif

absl::optional<std::string> GetDeviceManagerIdentity() {
  if (!policy::ManagementServiceFactory::GetForPlatform()->IsManaged())
    return absl::nullopt;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->IsActiveDirectoryManaged()
             ? connector->GetRealm()
             : connector->GetEnterpriseDomainManager();
#else
  return policy::GetManagedBy(g_browser_process->browser_policy_connector()
                                  ->machine_level_user_cloud_policy_manager());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
absl::optional<std::string> GetSessionManagerIdentity() {
  if (!policy::PolicyLoaderLacros::IsMainUserManaged())
    return absl::nullopt;
  return policy::PolicyLoaderLacros::main_user_policy_data()->managed_by();
}
#endif

absl::optional<std::string> GetAccountManagerIdentity(Profile* profile) {
  if (!policy::ManagementServiceFactory::GetForProfile(profile)->IsManaged())
    return absl::nullopt;

  const absl::optional<std::string> managed_by =
      policy::GetManagedBy(GetUserCloudPolicyManager(profile));
  if (managed_by)
    return *managed_by;

  return GetEnterpriseAccountDomain(profile);
}

}  // namespace chrome
