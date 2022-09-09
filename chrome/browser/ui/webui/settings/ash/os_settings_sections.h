// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_SECTIONS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_SECTIONS_H_

#include <unordered_map>
#include <vector>

// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/components/phonehub/phone_hub_manager.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "ash/webui/eche_app_ui/eche_app_manager.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/android_sms/android_sms_service.h"
// TODO(https://crbug.com/1164001): forward declare when moved ash
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
// TODO(https://crbug.com/1164001): forward declare when moved ash
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"

class ArcAppListPrefs;
class Profile;
class SupervisedUserService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace chromeos {
namespace settings {

// Collection of all OsSettingsSection implementations.
class OsSettingsSections {
 public:
  OsSettingsSections(
      Profile* profile,
      SearchTagRegistry* search_tag_registry,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::PhoneHubManager* phone_hub_manager,
      syncer::SyncService* sync_service,
      SupervisedUserService* supervised_user_service,
      KerberosCredentialsManager* kerberos_credentials_manager,
      ArcAppListPrefs* arc_app_list_prefs,
      signin::IdentityManager* identity_manager,
      android_sms::AndroidSmsService* android_sms_service,
      CupsPrintersManager* printers_manager,
      apps::AppServiceProxy* app_service_proxy,
      ash::eche_app::EcheAppManager* eche_app_manager);
  OsSettingsSections(const OsSettingsSections& other) = delete;
  OsSettingsSections& operator=(const OsSettingsSections& other) = delete;
  virtual ~OsSettingsSections();

  const OsSettingsSection* GetSection(mojom::Section section) const;

  std::vector<std::unique_ptr<OsSettingsSection>>& sections() {
    return sections_;
  }

 protected:
  // Used by tests.
  OsSettingsSections();

  std::unordered_map<mojom::Section, OsSettingsSection*> sections_map_;
  std::vector<std::unique_ptr<OsSettingsSection>> sections_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_SECTIONS_H_
