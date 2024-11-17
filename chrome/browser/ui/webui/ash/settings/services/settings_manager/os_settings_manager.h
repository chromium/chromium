// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_SETTINGS_MANAGER_OS_SETTINGS_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_SETTINGS_MANAGER_OS_SETTINGS_MANAGER_H_

#include <memory>

#include "ash/webui/common/backend/accelerator_fetcher.h"
#include "ash/webui/common/backend/shortcut_input_provider.h"
#include "ash/webui/eche_app_ui/eche_app_manager.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/input_device_settings/input_device_settings_provider.h"
#include "components/keyed_service/core/keyed_service.h"

class ArcAppListPrefs;
class Profile;

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

class CupsPrintersManager;
class KerberosCredentialsManager;

namespace local_search_service {
class LocalSearchServiceProxy;
}

namespace multidevice_setup {
class MultiDeviceSetupClient;
}

namespace phonehub {
class PhoneHubManager;
}

namespace settings {

class AppNotificationHandler;
class AppPermissionHandler;
class AppParentalControlsHandler;
class GraduationHandler;
class Hierarchy;
class OsSettingsSections;
class SearchHandler;
class SearchTagRegistry;
class SettingsUserActionTracker;

// Manager for the Chrome OS settings page. This class is implemented as a
// KeyedService, so one instance of the class is intended to be active for the
// lifetime of a logged-in user, even if the settings app is not opened.
//
// Main responsibilities:
//
// (1) Support search queries for settings content. OsSettingsManager is
//     responsible for updating the kCroSettings index of the
//     LocalSearchService
//     with search tags corresponding to all settings which are available.
//
//     The availability of settings depends on the user's account (e.g.,
//     Personalization settings are not available for guest accounts), the state
//     of the device (e.g., devices without an external monitor hide some
//     display settings), Enterprise settings (e.g., ARC++ is prohibited by some
//     policies), and the state of various flags and switches.
//
//     Whenever settings becomes available or unavailable, OsSettingsManager
//     updates the search index accordingly.
//
// (2) Provide static data to the settings app via the loadTimeData framework.
//     This includes localized strings required by the settings UI as well as
//     flags passed as booleans.
//
// (3) Add logic supporting message-passing between the browser process (C++)
//     and the settings app (JS), via SettingsPageUIHandler objects.
class OsSettingsManager : public KeyedService {
 public:
  OsSettingsManager(
      Profile* profile,
      local_search_service::LocalSearchServiceProxy* local_search_service_proxy,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::PhoneHubManager* phone_hub_manager,
      KerberosCredentialsManager* kerberos_credentials_manager,
      ArcAppListPrefs* arc_app_list_prefs,
      signin::IdentityManager* identity_manager,
      CupsPrintersManager* printers_manager,
      apps::AppServiceProxy* app_service_proxy,
      eche_app::EcheAppManager* eche_app_manager);
  OsSettingsManager(const OsSettingsManager& other) = delete;
  OsSettingsManager& operator=(const OsSettingsManager& other) = delete;
  ~OsSettingsManager() override;

  // Provides static data (i.e., localized strings and flag values) to an OS
  // settings instance. This function causes |html_source| to export a
  // strings.js file which contains a key-value map of the data added by this
  // function.
  void AddLoadTimeData(content::WebUIDataSource* html_source);

  // Adds SettingsPageUIHandlers to an OS settings instance.
  void AddHandlers(content::WebUI* web_ui);

  AppNotificationHandler* app_notification_handler() {
    return app_notification_handler_.get();
  }

  AppPermissionHandler* app_permission_handler() {
    return app_permission_handler_.get();
  }

  AppParentalControlsHandler* app_parental_controls_handler() {
    return app_parental_controls_handler_.get();
  }

  InputDeviceSettingsProvider* input_device_settings_provider() {
    return input_device_settings_provider_.get();
  }

  GraduationHandler* graduation_handler() { return graduation_handler_.get(); }

  DisplaySettingsProvider* display_settings_provider() {
    return display_settings_provider_.get();
  }

  SearchHandler* search_handler() { return search_handler_.get(); }

  SettingsUserActionTracker* settings_user_action_tracker() {
    return settings_user_action_tracker_.get();
  }

  AcceleratorFetcher* accelerator_fetcher() {
    return accelerator_fetcher_.get();
  }

  ShortcutInputProvider* shortcut_input_provider() {
    return shortcut_input_provider_.get();
  }

  const Hierarchy* hierarchy() const { return hierarchy_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(OsSettingsManagerTest, Initialization);

  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<SearchTagRegistry> search_tag_registry_;
  std::unique_ptr<OsSettingsSections> sections_;
  std::unique_ptr<Hierarchy> hierarchy_;
  std::unique_ptr<SettingsUserActionTracker> settings_user_action_tracker_;
  std::unique_ptr<SearchHandler> search_handler_;
  std::unique_ptr<AppNotificationHandler> app_notification_handler_;
  std::unique_ptr<AppPermissionHandler> app_permission_handler_;
  std::unique_ptr<AppParentalControlsHandler> app_parental_controls_handler_;
  std::unique_ptr<GraduationHandler> graduation_handler_;
  std::unique_ptr<InputDeviceSettingsProvider> input_device_settings_provider_;
  std::unique_ptr<DisplaySettingsProvider> display_settings_provider_;
  std::unique_ptr<ShortcutInputProvider> shortcut_input_provider_;
  std::unique_ptr<AcceleratorFetcher> accelerator_fetcher_;
};

}  // namespace settings
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_SETTINGS_MANAGER_OS_SETTINGS_MANAGER_H_
