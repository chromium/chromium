// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/url_handler_manager.h"
#include "chrome/browser/web_applications/components/url_handler_manager_impl.h"
#include "chrome/browser/web_applications/components/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/external_web_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/pending_app_manager_impl.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_migration_manager.h"
#include "chrome/browser/web_applications/web_app_mover.h"
#include "chrome/browser/web_applications/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

WebAppProvider::OsIntegrationManagerFactory
    g_os_integration_manager_factory_for_testing = nullptr;

}  // namespace

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForWebContents(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  return WebAppProvider::Get(profile);
}

// static
void WebAppProvider::SetOsIntegrationManagerFactoryForTesting(
    OsIntegrationManagerFactory factory) {
  g_os_integration_manager_factory_for_testing = factory;
}

WebAppProvider::WebAppProvider(Profile* profile) : profile_(profile) {
  DCHECK(AreWebAppsEnabled(profile_));
  // WebApp System must have only one instance in original profile.
  // Exclude secondary off-the-record profiles.
  DCHECK(!profile_->IsOffTheRecord());

  CreateCommonSubsystems(profile_);

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
    CreateWebAppsSubsystems(profile_);
  else
    CreateBookmarkAppsSubsystems(profile_);
}

WebAppProvider::~WebAppProvider() = default;

void WebAppProvider::Start() {
  CHECK(!started_);

  ConnectSubsystems();
  started_ = true;

  StartImpl();
}

AppRegistrar& WebAppProvider::registrar() {
  CheckIsConnected();
  return *registrar_;
}

AppRegistryController& WebAppProvider::registry_controller() {
  CheckIsConnected();
  return *registry_controller_;
}

InstallManager& WebAppProvider::install_manager() {
  CheckIsConnected();
  return *install_manager_;
}

InstallFinalizer& WebAppProvider::install_finalizer() {
  CheckIsConnected();
  return *install_finalizer_;
}

ManifestUpdateManager& WebAppProvider::manifest_update_manager() {
  CheckIsConnected();
  return *manifest_update_manager_;
}

PendingAppManager& WebAppProvider::pending_app_manager() {
  CheckIsConnected();
  return *pending_app_manager_;
}

WebAppPolicyManager& WebAppProvider::policy_manager() {
  CheckIsConnected();
  return *web_app_policy_manager_;
}

WebAppUiManager& WebAppProvider::ui_manager() {
  CheckIsConnected();
  return *ui_manager_;
}

WebAppAudioFocusIdMap& WebAppProvider::audio_focus_id_map() {
  CheckIsConnected();
  return *audio_focus_id_map_;
}

AppIconManager& WebAppProvider::icon_manager() {
  CheckIsConnected();
  return *icon_manager_;
}

SystemWebAppManager& WebAppProvider::system_web_app_manager() {
  CheckIsConnected();
  return *system_web_app_manager_;
}

OsIntegrationManager& WebAppProvider::os_integration_manager() {
  CheckIsConnected();
  return *os_integration_manager_;
}

void WebAppProvider::Shutdown() {
  ui_manager_->Shutdown();
  pending_app_manager_->Shutdown();
  manifest_update_manager_->Shutdown();
  system_web_app_manager_->Shutdown();
  install_manager_->Shutdown();
  icon_manager_->Shutdown();
  install_finalizer_->Shutdown();
  registrar_->Shutdown();
  if (web_app_mover_)
    web_app_mover_->Shutdown();
}

void WebAppProvider::StartImpl() {
  if (migration_manager_) {
    migration_manager_->StartDatabaseMigration(
        base::BindOnce(&WebAppProvider::OnDatabaseMigrationCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnDatabaseMigrationCompleted(/*success=*/true);
  }
}

void WebAppProvider::OnDatabaseMigrationCompleted(bool success) {
  StartRegistryController();
}

void WebAppProvider::CreateCommonSubsystems(Profile* profile) {
  audio_focus_id_map_ = std::make_unique<WebAppAudioFocusIdMap>();
  ui_manager_ = WebAppUiManager::Create(profile);
  install_manager_ = std::make_unique<WebAppInstallManager>(profile);
  manifest_update_manager_ = std::make_unique<ManifestUpdateManager>();
  pending_app_manager_ = std::make_unique<PendingAppManagerImpl>(profile);
  external_web_app_manager_ = std::make_unique<ExternalWebAppManager>(profile);
  system_web_app_manager_ = std::make_unique<SystemWebAppManager>(profile);
  web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(profile);
}

void WebAppProvider::CreateWebAppsSubsystems(Profile* profile) {
  database_factory_ = std::make_unique<WebAppDatabaseFactory>(profile);

  std::unique_ptr<WebAppRegistrar> registrar;
  std::unique_ptr<WebAppSyncBridge> sync_bridge;

  // Only WebAppSyncBridge must have an access to mutable WebAppRegistrar.
  {
    auto mutable_registrar = std::make_unique<WebAppRegistrarMutable>(profile);

    sync_bridge = std::make_unique<WebAppSyncBridge>(
        profile, database_factory_.get(), mutable_registrar.get(),
        install_manager_.get());

    // Upcast to read-only WebAppRegistrar.
    registrar = std::move(mutable_registrar);
  }

  auto legacy_finalizer = CreateBookmarkAppInstallFinalizer(profile);
  legacy_finalizer->SetSubsystems(/*registrar=*/nullptr,
                                  /*ui_manager=*/nullptr,
                                  /*registry_controller=*/nullptr,
                                  /*os_integration_manager=*/nullptr);

  auto icon_manager = std::make_unique<WebAppIconManager>(
      profile, *registrar, std::make_unique<FileUtilsWrapper>());
  install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
      profile, icon_manager.get(), std::move(legacy_finalizer));

  if (g_os_integration_manager_factory_for_testing) {
    os_integration_manager_ =
        g_os_integration_manager_factory_for_testing(profile);
  } else {
    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile);
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile);
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile, icon_manager.get(), file_handler_manager.get(),
        protocol_handler_manager.get());

    std::unique_ptr<UrlHandlerManager> url_handler_manager = nullptr;
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
    url_handler_manager = std::make_unique<UrlHandlerManagerImpl>(profile);
#endif

    os_integration_manager_ = std::make_unique<OsIntegrationManager>(
        profile, std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager), std::move(url_handler_manager));
  }

  migration_manager_ = std::make_unique<WebAppMigrationManager>(
      profile, database_factory_.get(), icon_manager.get(),
      os_integration_manager_.get());
  web_app_mover_ = WebAppMover::CreateIfNeeded(
      profile, registrar.get(), install_finalizer_.get(),
      install_manager_.get(), sync_bridge.get());

  // Upcast to unified subsystem types:
  registrar_ = std::move(registrar);
  registry_controller_ = std::move(sync_bridge);
  icon_manager_ = std::move(icon_manager);
}

void WebAppProvider::ConnectSubsystems() {
  DCHECK(!started_);

  install_finalizer_->SetSubsystems(registrar_.get(), ui_manager_.get(),
                                    registry_controller_.get(),
                                    os_integration_manager_.get());
  install_manager_->SetSubsystems(registrar_.get(),
                                  os_integration_manager_.get(),
                                  install_finalizer_.get());
  manifest_update_manager_->SetSubsystems(
      registrar_.get(), icon_manager_.get(), ui_manager_.get(),
      install_manager_.get(), system_web_app_manager_.get(),
      os_integration_manager_.get());
  pending_app_manager_->SetSubsystems(
      registrar_.get(), os_integration_manager_.get(), ui_manager_.get(),
      install_finalizer_.get(), install_manager_.get());
  external_web_app_manager_->SetSubsystems(pending_app_manager_.get());
  system_web_app_manager_->SetSubsystems(
      pending_app_manager_.get(), registrar_.get(), registry_controller_.get(),
      ui_manager_.get(), os_integration_manager_.get(),
      web_app_policy_manager_.get());
  web_app_policy_manager_->SetSubsystems(
      pending_app_manager_.get(), registrar_.get(), registry_controller_.get(),
      system_web_app_manager_.get(), os_integration_manager_.get());
  ui_manager_->SetSubsystems(registry_controller_.get(),
                             os_integration_manager_.get());
  os_integration_manager_->SetSubsystems(registrar_.get(), ui_manager_.get(),
                                         icon_manager_.get());
  registrar_->SetSubsystems(os_integration_manager_.get());
  registry_controller_->SetSubsystems(os_integration_manager_.get());

  connected_ = true;
}

void WebAppProvider::StartRegistryController() {
  registry_controller_->Init(
      base::BindOnce(&WebAppProvider::OnRegistryControllerReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppProvider::OnRegistryControllerReady() {
  DCHECK(!on_registry_ready_.is_signaled());

  registrar_->Start();
  install_finalizer_->Start();
  icon_manager_->Start();
  install_manager_->Start();
  external_web_app_manager_->Start();
  web_app_policy_manager_->Start();
  system_web_app_manager_->Start();
  manifest_update_manager_->Start();
  os_integration_manager_->Start();
  ui_manager_->Start();
  if (web_app_mover_)
    web_app_mover_->Start();

  on_registry_ready_.Signal();
}

void WebAppProvider::CheckIsConnected() const {
  DCHECK(connected_) << "Attempted to access Web App subsystem while "
                        "WebAppProvider is not connected.";
}

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ExternallyInstalledWebAppPrefs::RegisterProfilePrefs(registry);
  ExternalWebAppManager::RegisterProfilePrefs(registry);
  WebAppPolicyManager::RegisterProfilePrefs(registry);
  SystemWebAppManager::RegisterProfilePrefs(registry);
  WebAppPrefsUtilsRegisterProfilePrefs(registry);
  RegisterInstallBounceMetricProfilePrefs(registry);
  RegisterDailyWebAppMetricsProfilePrefs(registry);
}

}  // namespace web_app
