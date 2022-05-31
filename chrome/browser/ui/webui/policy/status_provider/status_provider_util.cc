// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/status_provider_util.h"

#include "google_apis/gaia/gaia_auth_util.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/off_hours/device_off_hours_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ui/managed_ui.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/enterprise/util/affiliation.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#endif

void ExtractDomainFromUsername(base::DictionaryValue* dict) {
  const std::string* username = dict->FindStringKey("username");
  if (username && !username->empty())
    dict->SetStringKey("domain", gaia::ExtractDomainName(*username));
}

void GetUserAffiliationStatus(base::DictionaryValue* dict, Profile* profile) {
  CHECK(profile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return;
  dict->SetBoolKey("isAffiliated", user->IsAffiliated());
#else
  // Don't show affiliation status if the browser isn't enrolled in CBCM.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile())
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  {
    if (!policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid())
      return;
  }
  dict->SetBoolKey("isAffiliated",
                   chrome::enterprise_util::IsProfileAffiliated(profile));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void GetOffHoursStatus(base::DictionaryValue* dict) {
  policy::off_hours::DeviceOffHoursController* off_hours_controller =
      ash::DeviceSettingsService::Get()->device_off_hours_controller();
  if (off_hours_controller) {
    dict->SetBoolKey("isOffHoursActive",
                     off_hours_controller->is_off_hours_mode());
  }
}

void GetUserManager(base::DictionaryValue* dict, Profile* profile) {
  CHECK(profile);

  absl::optional<std::string> account_manager =
      chrome::GetAccountManagerIdentity(profile);
  if (account_manager) {
    dict->SetStringKey("enterpriseDomainManager", *account_manager);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::string GetMachineStatusLegendKey() {
#if BUILDFLAG(IS_ANDROID)
  return "statusDevice";
#else
  return "statusMachine";
#endif  // BUILDFLAG(IS_ANDROID)
}
