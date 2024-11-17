// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_OS_SETTINGS_SECTIONS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_OS_SETTINGS_SECTIONS_H_

#include <unordered_map>
#include <vector>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"

class ArcAppListPrefs;
class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

class CupsPrintersManager;
class KerberosCredentialsManager;

namespace eche_app {
class EcheAppManager;
}  // namespace eche_app

namespace multidevice_setup {
class MultiDeviceSetupClient;
}

namespace phonehub {
class PhoneHubManager;
}

namespace settings {

// Collection of all OsSettingsSection implementations.
class OsSettingsSections {
 public:
  OsSettingsSections(
      Profile* profile,
      SearchTagRegistry* search_tag_registry,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::PhoneHubManager* phone_hub_manager,
      KerberosCredentialsManager* kerberos_credentials_manager,
      ArcAppListPrefs* arc_app_list_prefs,
      signin::IdentityManager* identity_manager,
      CupsPrintersManager* printers_manager,
      apps::AppServiceProxy* app_service_proxy,
      eche_app::EcheAppManager* eche_app_manager);
  OsSettingsSections(const OsSettingsSections& other) = delete;
  OsSettingsSections& operator=(const OsSettingsSections& other) = delete;
  virtual ~OsSettingsSections();

  const OsSettingsSection* GetSection(
      chromeos::settings::mojom::Section section) const;

  std::vector<std::unique_ptr<OsSettingsSection>>& sections() {
    return sections_;
  }

 protected:
  // Used by tests.
  OsSettingsSections();

  std::unordered_map<chromeos::settings::mojom::Section,
                     raw_ptr<OsSettingsSection, CtnExperimental>>
      sections_map_;
  std::vector<std::unique_ptr<OsSettingsSection>> sections_;

 private:
  void AddSection(chromeos::settings::mojom::Section section_id,
                  std::unique_ptr<OsSettingsSection> section);
};

}  // namespace settings
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_OS_SETTINGS_SECTIONS_H_
