// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"

#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/nearby_receive_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/pref_names.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/settings_user_action_tracker.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/os_settings_resources.h"
#include "chrome/grit/os_settings_resources_map.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/cellular_setup/cellular_setup_impl.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace chromeos {
namespace settings {

#if !BUILDFLAG(OPTIMIZE_WEBUI)
namespace {
const char kOsGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/settings/";
}
#endif

// static
void OSSettingsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSyncOsWallpaper, false);
}

OSSettingsUI::OSSettingsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      time_when_opened_(base::TimeTicks::Now()),
      webui_load_timer_(web_ui->GetWebContents(),
                        "ChromeOS.Settings.LoadDocumentTime",
                        "ChromeOS.Settings.LoadCompletedTime") {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);

  OsSettingsManager* manager = OsSettingsManagerFactory::GetForProfile(profile);
  manager->AddHandlers(web_ui);
  manager->AddLoadTimeData(html_source);
  html_source->DisableTrustedTypesCSP();

  // TODO(khorimoto): Move to DeviceSection::AddHandler() once |html_source|
  // parameter is removed.
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::StorageHandler>(profile,
                                                           html_source));

#if BUILDFLAG(OPTIMIZE_WEBUI)
  if (base::FeatureList::IsEnabled(::chromeos::features::kOsSettingsPolymer3)) {
    // Polymer3 Source files
    webui::SetupBundledWebUIDataSource(html_source, "chromeos/os_settings.js",
                                       IDR_OS_SETTINGS_OS_SETTINGS_ROLLUP_JS,
                                       IDR_OS_SETTINGS_OS_SETTINGS_V3_HTML);
    html_source->AddResourcePath("chromeos/shared.rollup.js",
                                 IDR_OS_SETTINGS_SHARED_ROLLUP_JS);
    html_source->AddResourcePath("chromeos/lazy_load.js",
                                 IDR_OS_SETTINGS_LAZY_LOAD_ROLLUP_JS);
  } else {
    // Polymer2 Source files
    html_source->AddResourcePath("crisper.js", IDR_OS_SETTINGS_CRISPER_JS);
    html_source->AddResourcePath("lazy_load.crisper.js",
                                 IDR_OS_SETTINGS_LAZY_LOAD_CRISPER_JS);
    html_source->AddResourcePath("chromeos/lazy_load.html",
                                 IDR_OS_SETTINGS_LAZY_LOAD_VULCANIZED_HTML);
    html_source->SetDefaultResource(IDR_OS_SETTINGS_VULCANIZED_HTML);
  }

  // We only need to register the mojo resources here because the rest are
  // bundled in.
  RegisterNearbySharedMojoResources(html_source);
#else
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kOsSettingsResources, kOsSettingsResourcesSize),
      kOsGeneratedPath,
      base::FeatureList::IsEnabled(chromeos::features::kOsSettingsPolymer3)
          ? IDR_OS_SETTINGS_OS_SETTINGS_V3_HTML
          : IDR_OS_SETTINGS_SETTINGS_HTML);

  // Register chrome://nearby resources so they are available at
  // chrome://os-settings. This allows the sharing of resources without having
  // to put everything in chrome://resources. This is necessary because portions
  // of the nearby UI need to be re-used in both places.
  // This is not nessary when OPTIMIZE_WEBUI is true because the files will be
  // added to the optimized bundles.
  RegisterNearbySharedResources(html_source);
#endif

  ManagedUIHandler::Initialize(web_ui, html_source);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
}

OSSettingsUI::~OSSettingsUI() {
  // Note: OSSettingsUI lifetime is tied to the lifetime of the browser window.
  base::UmaHistogramCustomTimes("ChromeOS.Settings.WindowOpenDuration",
                                base::TimeTicks::Now() - time_when_opened_,
                                /*min=*/base::TimeDelta::FromMicroseconds(500),
                                /*max=*/base::TimeDelta::FromHours(1),
                                /*buckets=*/50);
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<cellular_setup::mojom::CellularSetup> receiver) {
  cellular_setup::CellularSetupImpl::CreateAndBindToReciever(
      std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<mojom::UserActionRecorder> receiver) {
  OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->settings_user_action_tracker()
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<mojom::SearchHandler> receiver) {
  OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->search_handler()
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<app_management::mojom::PageHandlerFactory> receiver) {
  if (!app_management_page_handler_factory_) {
    app_management_page_handler_factory_ =
        std::make_unique<AppManagementPageHandlerFactory>(
            Profile::FromWebUI(web_ui()));
  }
  app_management_page_handler_factory_->Bind(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<nearby_share::mojom::NearbyShareSettings> receiver) {
  NearbySharingService* service =
      NearbySharingServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  service->GetSettings()->Bind(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<nearby_share::mojom::ReceiveManager> receiver) {
  NearbySharingService* service =
      NearbySharingServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  mojo::MakeSelfOwnedReceiver(std::make_unique<NearbyReceiveManager>(service),
                              std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver) {
  NearbySharingService* service =
      NearbySharingServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  service->GetContactManager()->Bind(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OSSettingsUI)

}  // namespace settings
}  // namespace chromeos
