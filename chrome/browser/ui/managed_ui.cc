// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace chrome {

bool ShouldDisplayManagedUi(Profile* profile) {
#if defined(OS_CHROMEOS)
  // Don't show the UI in demo mode.
  if (chromeos::DemoSession::IsDeviceInDemoMode())
    return false;

  // Don't show the UI for Unicorn accounts.
  if (profile->IsSupervised())
    return false;
#endif

  // This profile may have policies configured.
  auto* profile_connector = profile->GetProfilePolicyConnector();
  if (profile_connector->IsManaged())
    return true;

#if defined(OS_CHROMEOS)
  // This session's primary user may also have policies, and those policies may
  // not have per-profile support.
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user) {
    auto* primary_profile =
        chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
    if (primary_profile) {
      auto* primary_profile_connector =
          primary_profile->GetProfilePolicyConnector();
      if (primary_profile_connector->IsManaged())
        return true;
    }
  }

  // The machine may be enrolled, via Google Cloud or Active Directory.
  auto* browser_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (browser_connector->IsEnterpriseManaged())
    return true;
#else
  // There may be policies set in a platform-specific way (e.g. Windows
  // Registry), or with machine level user cloud policies.
  auto* browser_connector = g_browser_process->browser_policy_connector();
  if (browser_connector->HasMachineLevelPolicies())
    return true;
#endif

  return false;
}

base::string16 GetManagedUiMenuItemLabel(Profile* profile) {
  std::string management_domain =
      ManagementUIHandler::GetAccountDomain(profile);

  int string_id = IDS_MANAGED;
  std::vector<base::string16> replacements;
  if (!management_domain.empty()) {
    string_id = IDS_MANAGED_BY;
    replacements.push_back(base::UTF8ToUTF16(management_domain));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

base::string16 GetManagedUiWebUILabel(Profile* profile) {
  std::string management_domain =
      ManagementUIHandler::GetAccountDomain(profile);

  int string_id = IDS_MANAGED_WITH_HYPERLINK;

  std::vector<base::string16> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  if (!management_domain.empty()) {
    string_id = IDS_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(management_domain));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

#if defined(OS_CHROMEOS)
base::string16 GetDeviceManagedUiWebUILabel(Profile* profile) {
  std::string management_domain =
      ManagementUIHandler::GetAccountDomain(profile);

  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;

  std::vector<base::string16> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  replacements.push_back(ui::GetChromeOSDeviceName());
  if (!management_domain.empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(management_domain));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#endif

}  // namespace chrome
