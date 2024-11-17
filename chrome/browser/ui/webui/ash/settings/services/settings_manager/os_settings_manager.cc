// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager.h"

#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/webui/common/backend/accelerator_fetcher.h"
#include "ash/webui/common/backend/shortcut_input_provider.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_notification_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_parental_controls_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/input_device_settings/input_device_settings_provider.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_sections.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/graduation_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/app_permission_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/hierarchy.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/ash/settings/services/metrics/settings_user_action_tracker.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::settings {

OsSettingsManager::OsSettingsManager(
    Profile* profile,
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    phonehub::PhoneHubManager* phone_hub_manager,
    KerberosCredentialsManager* kerberos_credentials_manager,
    ArcAppListPrefs* arc_app_list_prefs,
    signin::IdentityManager* identity_manager,
    CupsPrintersManager* printers_manager,
    apps::AppServiceProxy* app_service_proxy,
    eche_app::EcheAppManager* eche_app_manager)
    : search_tag_registry_(
          std::make_unique<SearchTagRegistry>(local_search_service_proxy)),
      sections_(
          std::make_unique<OsSettingsSections>(profile,
                                               search_tag_registry_.get(),
                                               multidevice_setup_client,
                                               phone_hub_manager,
                                               kerberos_credentials_manager,
                                               arc_app_list_prefs,
                                               identity_manager,
                                               printers_manager,
                                               app_service_proxy,
                                               eche_app_manager)),
      hierarchy_(std::make_unique<Hierarchy>(sections_.get())),
      settings_user_action_tracker_(
          std::make_unique<SettingsUserActionTracker>(hierarchy_.get(),
                                                      sections_.get(),
                                                      profile->GetPrefs())),
      search_handler_(
          std::make_unique<SearchHandler>(search_tag_registry_.get(),
                                          sections_.get(),
                                          hierarchy_.get(),
                                          local_search_service_proxy)),
      app_notification_handler_(
          std::make_unique<AppNotificationHandler>(app_service_proxy)),
      app_permission_handler_(
          std::make_unique<AppPermissionHandler>(app_service_proxy)),
      app_parental_controls_handler_(
          std::make_unique<AppParentalControlsHandler>(app_service_proxy,
                                                       profile)),
      graduation_handler_(std::make_unique<GraduationHandler>(profile)),
      input_device_settings_provider_(
          std::make_unique<InputDeviceSettingsProvider>()),
      display_settings_provider_(std::make_unique<DisplaySettingsProvider>()),
      shortcut_input_provider_(std::make_unique<ShortcutInputProvider>()),
      accelerator_fetcher_(std::make_unique<AcceleratorFetcher>()) {}

OsSettingsManager::~OsSettingsManager() = default;

void OsSettingsManager::AddLoadTimeData(content::WebUIDataSource* html_source) {
  for (const auto& section : sections_->sections()) {
    section->AddLoadTimeData(html_source);
  }

  html_source->AddBoolean("isCrosComponentsEnabled",
                          chromeos::features::IsCrosComponentsEnabled());
  html_source->UseStringsJs();
}

void OsSettingsManager::AddHandlers(content::WebUI* web_ui) {
  for (const auto& section : sections_->sections()) {
    section->AddHandlers(web_ui);
  }
}

void OsSettingsManager::Shutdown() {
  // Note: These must be deleted in the opposite order of their creation to
  // prevent against UAF violations.
  accelerator_fetcher_.reset();
  shortcut_input_provider_.reset();
  display_settings_provider_.reset();
  input_device_settings_provider_.reset();
  graduation_handler_.reset();
  app_notification_handler_.reset();
  app_permission_handler_.reset();
  search_handler_.reset();
  settings_user_action_tracker_.reset();
  hierarchy_.reset();
  sections_.reset();
  search_tag_registry_.reset();
}

}  // namespace ash::settings
