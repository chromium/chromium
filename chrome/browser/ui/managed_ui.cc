// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace chrome {

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

base::string16 GetManagedUiMenuItemLabel(Profile* profile) {
  std::string account_manager = ManagementUIHandler::GetAccountManager(profile);

  int string_id = IDS_MANAGED;
  std::vector<base::string16> replacements;
  if (!account_manager.empty()) {
    string_id = IDS_MANAGED_BY;
    replacements.push_back(base::UTF8ToUTF16(account_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

base::string16 GetManagedUiWebUILabel(Profile* profile) {
  std::string account_manager = ManagementUIHandler::GetAccountManager(profile);

  int string_id = IDS_MANAGED_WITH_HYPERLINK;

  std::vector<base::string16> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  if (!account_manager.empty()) {
    string_id = IDS_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(account_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
base::string16 GetDeviceManagedUiWebUILabel() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const std::string device_manager =
      connector->IsActiveDirectoryManaged()
          ? connector->GetRealm()
          : connector->GetEnterpriseDomainManager();

  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;

  std::vector<base::string16> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  replacements.push_back(ui::GetChromeOSDeviceName());
  if (!device_manager.empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(device_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#endif

}  // namespace chrome
