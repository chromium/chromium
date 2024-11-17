// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/settings/os_settings_ui.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/audio_config_service.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/cpp/connectivity_services.h"
#include "ash/public/cpp/esim_manager.h"
#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_service.h"
#include "ash/webui/common/mojom/accelerator_fetcher.mojom.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_handler.h"
#include "base/metrics/histogram_functions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/nearby_receive_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_notification_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_parental_controls_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/graduation_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/app_permission_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/magic_boost_notice_page_handler_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/storage/device_storage_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_handler.h"
#include "chrome/browser/ui/webui/ash/settings/services/hats/os_settings_hats_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/hats/os_settings_hats_manager_factory.h"
#include "chrome/browser/ui/webui/ash/settings/services/metrics/settings_user_action_tracker.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/os_settings_resources.h"
#include "chrome/grit/os_settings_resources_map.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/cellular_setup/cellular_setup_impl.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/ime_service.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "chromeos/components/in_session_auth/in_process_instances.h"
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

#if !BUILDFLAG(OPTIMIZE_WEBUI)
#include "chrome/grit/settings_shared_resources.h"
#include "chrome/grit/settings_shared_resources_map.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/nearby_sharing/internal/icons/grit/nearby_share_internal_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

class AppManagementDelegate : public AppManagementPageHandlerBase::Delegate {
 public:
  AppManagementDelegate() = default;
  ~AppManagementDelegate() override = default;

  gfx::NativeWindow GetUninstallAnchorWindow() const override {
    return nullptr;
  }
};

}  // namespace

namespace ash::settings {

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
      content::WebUIDataSource::CreateAndAdd(profile,
                                             chrome::kChromeUIOSSettingsHost);
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
  OsSettingsManager* manager = OsSettingsManagerFactory::GetForProfile(profile);
  manager->AddHandlers(web_ui);
  manager->AddLoadTimeData(html_source);

  // TODO(b/300151715): Move to StorageSection::AddHandler() once/if
  // |html_source| parameter is removed.
  web_ui->AddMessageHandler(
      std::make_unique<StorageHandler>(profile, html_source));

  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kOsSettingsResources, kOsSettingsResourcesSize),
      IDR_OS_SETTINGS_OS_SETTINGS_HTML);
  ash::EnableTrustedTypesCSP(html_source);

#if !BUILDFLAG(OPTIMIZE_WEBUI)
  html_source->AddResourcePaths(
      base::make_span(kSettingsSharedResources, kSettingsSharedResourcesSize));
#endif

  // Flag for using updated icons in search results and pages.
  html_source->AddBoolean("isNameEnabled", ::features::IsNameEnabled());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  html_source->AddResourcePath("nearby/nearby-share-internal-icons.html",
                               IDR_NEARBY_SHARE_INTERNAL_ICONS);
  html_source->AddResourcePath("nearby/nearby-share-internal-icons.m.js",
                               IDR_NEARBY_SHARE_INTERNAL_ICONS_M_JS);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // To use lottie, the worker-src CSP needs to be updated for the web ui that
  // is using it. Since as of now there are only a couple of webuis using
  // lottie animations, this update has to be performed manually. As the usage
  // increases, set this as the default so manual override is no longer
  // required.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");

  ManagedUIHandler::Initialize(web_ui, html_source);
}

OSSettingsUI::~OSSettingsUI() {
  // Note: OSSettingsUI lifetime is tied to the lifetime of the browser window.
  base::UmaHistogramCustomTimes("ChromeOS.Settings.WindowOpenDuration",
                                base::TimeTicks::Now() - time_when_opened_,
                                /*min=*/base::Microseconds(500),
                                /*max=*/base::Hours(1),
                                /*buckets=*/50);

  // Sends request for OsSettingsHats notification upon shutting down the app.
  OsSettingsHatsManager* settingsHatsManager =
      OsSettingsHatsManagerFactory::GetInstance()->GetForProfile(
          Profile::FromWebUI(web_ui()));
  settingsHatsManager->MaybeSendSettingsHats();

  // OsSettingsHatsManager records whether the user used the Search
  // functionality per each session that Settings app has opened up. When the
  // Settings app is closed, OsSettingsHatsManager will remain alive in the
  // background and the state remains stored in the manager, so we will reset
  // that knowledge.
  settingsHatsManager->SetSettingsUsedSearch(false);

  // Resets the tracking of device IDs associated with notification clicks.
  // This method is called when the Settings app is closed to prevent the
  // recording of metrics if a user changes settings long after clicking a
  // notification.
  InputDeviceSettingsController::Get()->ResetNotificationDeviceTracking();
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<cellular_setup::mojom::CellularSetup> receiver) {
  cellular_setup::CellularSetupImpl::CreateAndBindToReciever(
      std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver) {
  GetESimManager(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  GetNetworkConfigService(std::move(receiver));
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
    mojo::PendingReceiver<personalization_app::mojom::SearchHandler> receiver) {
  auto* profile = Profile::FromWebUI(web_ui());
  DCHECK(personalization_app::CanSeeWallpaperOrPersonalizationApp(profile));

  auto* search_handler = personalization_app::PersonalizationAppManagerFactory::
                             GetForBrowserContext(profile)
                                 ->search_handler();
  DCHECK(search_handler);
  search_handler->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<app_notification::mojom::AppNotificationsHandler>
        receiver) {
  OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->app_notification_handler()
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<
        app_parental_controls::mojom::AppParentalControlsHandler> receiver) {
  OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->app_parental_controls_handler()
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<app_permission::mojom::AppPermissionsHandler>
        receiver) {
  OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->app_permission_handler()
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<app_management::mojom::PageHandlerFactory> receiver) {
  if (!app_management_page_handler_factory_) {
    auto delegate = std::make_unique<AppManagementDelegate>();
    app_management_page_handler_factory_ =
        std::make_unique<AppManagementPageHandlerFactory>(
            Profile::FromWebUI(web_ui()), std::move(delegate));
  }
  app_management_page_handler_factory_->Bind(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<nearby_share::mojom::NearbyShareSettings> receiver) {
  if (!NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          Profile::FromWebUI(web_ui()))) {
    return;
  }

  NearbySharingService* service =
      NearbySharingServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  service->GetSettings()->Bind(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<nearby_share::mojom::ReceiveManager> receiver) {
  if (!NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          Profile::FromWebUI(web_ui()))) {
    return;
  }

  NearbySharingService* service =
      NearbySharingServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  mojo::MakeSelfOwnedReceiver(std::make_unique<NearbyReceiveManager>(service),
                              std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver) {
  if (!NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          Profile::FromWebUI(web_ui()))) {
    return;
  }

  NearbySharingService* service =
      NearbySharingServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  service->GetContactManager()->Bind(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<bluetooth_config::mojom::CrosBluetoothConfig>
        receiver) {
  GetBluetoothConfigService(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<hotspot_config::mojom::CrosHotspotConfig> receiver) {
  GetHotspotConfigService(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<audio_config::mojom::CrosAudioConfig> receiver) {
  GetAudioConfigService(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<graduation::mojom::GraduationHandler> receiver) {
  OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->graduation_handler()
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<mojom::InputDeviceSettingsProvider> receiver) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  auto* provider =
      OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
          ->input_device_settings_provider();
  if (features::IsPeripheralCustomizationEnabled() ||
      ::features::IsAccessibilityFaceGazeEnabled()) {
    provider->Initialize(web_ui());
  }
  provider->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<mojom::DisplaySettingsProvider> receiver) {
  OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->display_settings_provider()
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<::ash::common::mojom::AcceleratorFetcher> receiver) {
  CHECK(::features::IsShortcutCustomizationEnabled());
  OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
      ->accelerator_fetcher()
      ->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<::ash::common::mojom::ShortcutInputProvider>
        receiver) {
  CHECK(features::IsPeripheralCustomizationEnabled() ||
        ::features::IsAccessibilityFaceGazeEnabled());
  auto* shortcut_input_provider =
      OsSettingsManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
          ->shortcut_input_provider();
  auto* widget = views::Widget::GetWidgetForNativeWindow(
      web_ui()->GetWebContents()->GetTopLevelNativeWindow());
  if (widget) {
    shortcut_input_provider->TieProviderToWidget(widget);
  }
  shortcut_input_provider->BindInterface(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<auth::mojom::AuthFactorConfig> receiver) {
  auth::BindToAuthFactorConfig(std::move(receiver),
                               quick_unlock::QuickUnlockFactory::GetDelegate(),
                               g_browser_process->local_state());
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<auth::mojom::RecoveryFactorEditor> receiver) {
  auth::BindToRecoveryFactorEditor(
      std::move(receiver), quick_unlock::QuickUnlockFactory::GetDelegate(),
      g_browser_process->local_state());
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<auth::mojom::PinFactorEditor> receiver) {
  auto* pin_backend = quick_unlock::PinBackend::GetInstance();
  CHECK(pin_backend);
  auth::BindToPinFactorEditor(std::move(receiver),
                              quick_unlock::QuickUnlockFactory::GetDelegate(),
                              g_browser_process->local_state(), *pin_backend);
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<auth::mojom::PasswordFactorEditor> receiver) {
  auth::BindToPasswordFactorEditor(
      std::move(receiver), quick_unlock::QuickUnlockFactory::GetDelegate(),
      g_browser_process->local_state());
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver) {
  chromeos::auth::BindToInSessionAuthService(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<google_drive::mojom::PageHandlerFactory> receiver) {
  // The PageHandlerFactory is reused across same-origin navigations, so ensure
  // any existing factories are reset.
  google_drive_page_handler_factory_.reset();
  google_drive_page_handler_factory_ =
      std::make_unique<GoogleDrivePageHandlerFactory>(
          Profile::FromWebUI(web_ui()), std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<one_drive::mojom::PageHandlerFactory> receiver) {
  one_drive_page_handler_factory_ =
      std::make_unique<OneDrivePageHandlerFactory>(Profile::FromWebUI(web_ui()),
                                                   std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
        receiver) {
  ash::GetPasspointService(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<ash::mojom::HidPreservingBluetoothStateController>
        receiver) {
  DCHECK(features::IsBluetoothDisconnectWarningEnabled());
  GetHidPreservingBluetoothStateControllerService(std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<ash::ime::mojom::InputMethodUserDataService>
        receiver) {
  input_method::InputMethodManager::Get()->BindInputMethodUserDataService(
      std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<date_time::mojom::PageHandlerFactory> receiver) {
  date_time_handler_factory_ = std::make_unique<DateTimeHandlerFactory>(
      web_ui(), Profile::FromWebUI(web_ui()), std::move(receiver));
}

void OSSettingsUI::BindInterface(
    mojo::PendingReceiver<magic_boost_handler::mojom::PageHandlerFactory>
        receiver) {
  magic_boost_notice_page_handler_factory_ =
      std::make_unique<MagicBoostNoticePageHandlerFactory>(
          Profile::FromWebUI(web_ui()), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OSSettingsUI)

}  // namespace ash::settings
