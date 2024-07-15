// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"

namespace ash::settings {

// static
OsSettingsManager* OsSettingsManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<OsSettingsManager*>(
      OsSettingsManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
OsSettingsManagerFactory* OsSettingsManagerFactory::GetInstance() {
  static base::NoDestructor<OsSettingsManagerFactory> instance;
  return instance.get();
}

OsSettingsManagerFactory::OsSettingsManagerFactory()
    : ProfileKeyedServiceFactory(
          "OsSettingsManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(
      local_search_service::LocalSearchServiceProxyFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
  DependsOn(phonehub::PhoneHubManagerFactory::GetInstance());
  DependsOn(KerberosCredentialsManagerFactory::GetInstance());
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(CupsPrintersManagerFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
  DependsOn(eche_app::EcheAppManagerFactory::GetInstance());
}

OsSettingsManagerFactory::~OsSettingsManagerFactory() = default;

std::unique_ptr<KeyedService>
OsSettingsManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return std::make_unique<OsSettingsManager>(
      profile,
      local_search_service::LocalSearchServiceProxyFactory::
          GetForBrowserContext(context),
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(profile),
      phonehub::PhoneHubManagerFactory::GetForProfile(profile),
      KerberosCredentialsManagerFactory::Get(profile),
      ArcAppListPrefsFactory::GetForBrowserContext(profile),
      IdentityManagerFactory::GetForProfile(profile),
      CupsPrintersManagerFactory::GetForBrowserContext(profile),
      apps::AppServiceProxyFactory::GetForProfile(profile),
      eche_app::EcheAppManagerFactory::GetForProfile(profile));
}

bool OsSettingsManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash::settings
