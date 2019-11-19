// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"
#include "chrome/browser/web_applications/components/manifest_update_manager.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/components/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_file_handler_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_icon_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registry_controller.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_manager.h"
#include "chrome/browser/web_applications/external_web_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/pending_app_manager_impl.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

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

FileHandlerManager& WebAppProvider::file_handler_manager() {
  CheckIsConnected();
  return *file_handler_manager_;
}

AppIconManager& WebAppProvider::icon_manager() {
  CheckIsConnected();
  return *icon_manager_;
}

AppShortcutManager& WebAppProvider::shortcut_manager() {
  CheckIsConnected();
  return *shortcut_manager_;
}

SystemWebAppManager& WebAppProvider::system_web_app_manager() {
  CheckIsConnected();
  return *system_web_app_manager_;
}

void WebAppProvider::Shutdown() {
  pending_app_manager_->Shutdown();
  install_manager_->Shutdown();
  manifest_update_manager_->Shutdown();
}

void WebAppProvider::StartImpl() {
  StartRegistryController();
}

void WebAppProvider::CreateCommonSubsystems(Profile* profile) {
  audio_focus_id_map_ = std::make_unique<WebAppAudioFocusIdMap>();
  ui_manager_ = WebAppUiManager::Create(profile);
  install_manager_ = std::make_unique<WebAppInstallManager>(profile);
  manifest_update_manager_ = std::make_unique<ManifestUpdateManager>(profile);
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

  auto icon_manager = std::make_unique<WebAppIconManager>(
      profile, *registrar, std::make_unique<FileUtilsWrapper>());
  install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
      sync_bridge.get(), icon_manager.get());
  file_handler_manager_ = std::make_unique<WebAppFileHandlerManager>(profile);
  shortcut_manager_ = std::make_unique<WebAppShortcutManager>(profile);

  // Upcast to unified subsystem types:
  registrar_ = std::move(registrar);
  registry_controller_ = std::move(sync_bridge);
  icon_manager_ = std::move(icon_manager);
}

void WebAppProvider::CreateBookmarkAppsSubsystems(Profile* profile) {
  registrar_ = std::make_unique<extensions::BookmarkAppRegistrar>(profile);
  registry_controller_ =
      std::make_unique<extensions::BookmarkAppRegistryController>(profile);
  icon_manager_ = std::make_unique<extensions::BookmarkAppIconManager>(profile);
  install_finalizer_ =
      std::make_unique<extensions::BookmarkAppInstallFinalizer>(profile);
  file_handler_manager_ =
      std::make_unique<extensions::BookmarkAppFileHandlerManager>(profile);
  shortcut_manager_ =
      std::make_unique<extensions::BookmarkAppShortcutManager>(profile);
}

void WebAppProvider::ConnectSubsystems() {
  DCHECK(!started_);

  install_finalizer_->SetSubsystems(registrar_.get(), ui_manager_.get());
  install_manager_->SetSubsystems(registrar_.get(), shortcut_manager_.get(),
                                  install_finalizer_.get());
  manifest_update_manager_->SetSubsystems(registrar_.get(), ui_manager_.get(),
                                          install_manager_.get());
  pending_app_manager_->SetSubsystems(registrar_.get(), shortcut_manager_.get(),
                                      ui_manager_.get(),
                                      install_finalizer_.get());
  external_web_app_manager_->SetSubsystems(pending_app_manager_.get());
  system_web_app_manager_->SetSubsystems(pending_app_manager_.get(),
                                         registrar_.get(), ui_manager_.get());
  web_app_policy_manager_->SetSubsystems(pending_app_manager_.get());
  file_handler_manager_->SetSubsystems(registrar_.get(),
                                       shortcut_manager_.get());
  shortcut_manager_->SetSubsystems(registrar_.get());

  connected_ = true;
}

void WebAppProvider::StartRegistryController() {
  registry_controller_->Init(
      base::BindOnce(&WebAppProvider::OnRegistryControllerReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppProvider::OnRegistryControllerReady() {
  DCHECK(!on_registry_ready_.is_signaled());

  // TODO(crbug.com/877898): Port all these managers to support BMO. Start them.
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    external_web_app_manager_->Start();
    web_app_policy_manager_->Start();
    system_web_app_manager_->Start();
  }
  manifest_update_manager_->Start();
  file_handler_manager_->Start();

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
  WebAppPolicyManager::RegisterProfilePrefs(registry);
  SystemWebAppManager::RegisterProfilePrefs(registry);
  RegisterInstallBounceMetricProfilePrefs(registry);
}

}  // namespace web_app
