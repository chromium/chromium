// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_settings_manager.h"

#include "chrome/browser/ui/webui/settings/ash/hierarchy.h"
#include "chrome/browser/ui/webui/settings/ash/os_apps_page/app_notification_handler.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_sections.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_handler.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/ash/settings_user_action_tracker.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::settings {

OsSettingsManager::OsSettingsManager(
    Profile* profile,
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy,
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
    eche_app::EcheAppManager* eche_app_manager)
    : search_tag_registry_(
          std::make_unique<SearchTagRegistry>(local_search_service_proxy)),
      sections_(
          std::make_unique<OsSettingsSections>(profile,
                                               search_tag_registry_.get(),
                                               multidevice_setup_client,
                                               phone_hub_manager,
                                               sync_service,
                                               supervised_user_service,
                                               kerberos_credentials_manager,
                                               arc_app_list_prefs,
                                               identity_manager,
                                               android_sms_service,
                                               printers_manager,
                                               app_service_proxy,
                                               eche_app_manager)),
      hierarchy_(std::make_unique<Hierarchy>(sections_.get())),
      settings_user_action_tracker_(
          std::make_unique<SettingsUserActionTracker>(hierarchy_.get(),
                                                      sections_.get())),
      search_handler_(
          std::make_unique<SearchHandler>(search_tag_registry_.get(),
                                          sections_.get(),
                                          hierarchy_.get(),
                                          local_search_service_proxy)),
      app_notification_handler_(
          std::make_unique<AppNotificationHandler>(app_service_proxy)) {}

OsSettingsManager::~OsSettingsManager() = default;

void OsSettingsManager::AddLoadTimeData(content::WebUIDataSource* html_source) {
  for (const auto& section : sections_->sections())
    section->AddLoadTimeData(html_source);
  html_source->UseStringsJs();
}

void OsSettingsManager::AddHandlers(content::WebUI* web_ui) {
  for (const auto& section : sections_->sections())
    section->AddHandlers(web_ui);
}

void OsSettingsManager::Shutdown() {
  // Note: These must be deleted in the opposite order of their creation to
  // prevent against UAF violations.
  app_notification_handler_.reset();
  search_handler_.reset();
  settings_user_action_tracker_.reset();
  hierarchy_.reset();
  sections_.reset();
  search_tag_registry_.reset();
}

}  // namespace ash::settings
