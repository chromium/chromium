// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/url_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

WebAppProvider::OsIntegrationManagerFactory
    g_os_integration_manager_factory_for_testing = nullptr;

}  // namespace

// static
WebAppProvider* WebAppProvider::GetDeprecated(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForWebApps(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If features::kWebAppsCrosapi is enabled, Ash browser only manages system
  // web apps (return nullptr here). Otherwise, Ash browser manages all web apps
  // (return WebAppProvider).
  return IsWebAppsCrosapiEnabled()
             ? nullptr
             : WebAppProviderFactory::GetForProfile(profile);
#else
  return WebAppProviderFactory::GetForProfile(profile);
#endif
}

// static
WebAppProvider* WebAppProvider::GetForLocalAppsUnchecked(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForTest(Profile* profile) {
  // Running a nested base::RunLoop outside of tests causes a deadlock. Crash
  // immediately instead of deadlocking for easier debugging (especially for
  // TAST tests which use prod binaries).
  CHECK_IS_TEST();

  WebAppProvider* provider = GetForLocalAppsUnchecked(profile);
  if (!provider)
    return nullptr;

  if (provider->on_registry_ready().is_signaled())
    return provider;

  base::RunLoop run_loop;
  provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  return provider;
}

// static
WebAppProvider* WebAppProvider::GetForWebContents(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  return WebAppProvider::GetForLocalAppsUnchecked(profile);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!profile_->IsGuestSession())
    DCHECK(!profile_->IsOffTheRecord());
#else
  DCHECK(!profile_->IsOffTheRecord());
#endif

  CreateSubsystems(profile_);
}

WebAppProvider::~WebAppProvider() = default;

void WebAppProvider::Start() {
  CHECK(!started_);

  ConnectSubsystems();
  started_ = true;

  StartImpl();
}

WebAppRegistrar& WebAppProvider::registrar_unsafe() {
  CheckIsConnected();
  return *registrar_;
}

const WebAppRegistrar& WebAppProvider::registrar_unsafe() const {
  CheckIsConnected();
  return *registrar_;
}

WebAppSyncBridge& WebAppProvider::sync_bridge_unsafe() {
  CheckIsConnected();
  return *sync_bridge_;
}

WebAppInstallManager& WebAppProvider::install_manager() {
  CheckIsConnected();
  return *install_manager_;
}

WebAppInstallFinalizer& WebAppProvider::install_finalizer() {
  CheckIsConnected();
  return *install_finalizer_;
}

ManifestUpdateManager& WebAppProvider::manifest_update_manager() {
  CheckIsConnected();
  return *manifest_update_manager_;
}

ExternallyManagedAppManager& WebAppProvider::externally_managed_app_manager() {
  CheckIsConnected();
  return *externally_managed_app_manager_;
}

WebAppPolicyManager& WebAppProvider::policy_manager() {
  CheckIsConnected();
  return *web_app_policy_manager_;
}

IsolatedWebAppCommandLineInstallManager&
WebAppProvider::iwa_command_line_install_manager() {
  CheckIsConnected();
  return *iwa_command_line_install_manager_;
}

WebAppUiManager& WebAppProvider::ui_manager() {
  CheckIsConnected();
  return *ui_manager_;
}

WebAppAudioFocusIdMap& WebAppProvider::audio_focus_id_map() {
  CheckIsConnected();
  return *audio_focus_id_map_;
}

WebAppIconManager& WebAppProvider::icon_manager() {
  CheckIsConnected();
  return *icon_manager_;
}

WebAppTranslationManager& WebAppProvider::translation_manager() {
  CheckIsConnected();
  return *translation_manager_;
}

OsIntegrationManager& WebAppProvider::os_integration_manager() {
  CheckIsConnected();
  return *os_integration_manager_;
}

const OsIntegrationManager& WebAppProvider::os_integration_manager() const {
  CheckIsConnected();
  return *os_integration_manager_;
}

WebAppOriginAssociationManager& WebAppProvider::origin_association_manager() {
  return *origin_association_manager_;
}

WebAppCommandManager& WebAppProvider::command_manager() {
  // Note: It is OK to access the command manager before connection or start.
  // Internally it will queue commands to only happen after it has started.
  return *command_manager_;
}

WebAppCommandScheduler& WebAppProvider::scheduler() {
  return *command_scheduler_;
}

void WebAppProvider::Shutdown() {
  command_scheduler_->Shutdown();
  command_manager_->Shutdown();
  ui_manager_->Shutdown();
  externally_managed_app_manager_->Shutdown();
  manifest_update_manager_->Shutdown();
  iwa_command_line_install_manager_->Shutdown();
  install_manager_->Shutdown();
  icon_manager_->Shutdown();
  install_finalizer_->Shutdown();
  registrar_->Shutdown();
  is_registry_ready_ = false;
}

void WebAppProvider::StartImpl() {
  StartSyncBridge();
}

void WebAppProvider::CreateSubsystems(Profile* profile) {
  audio_focus_id_map_ = std::make_unique<WebAppAudioFocusIdMap>();
  ui_manager_ = WebAppUiManager::Create(profile);
  install_manager_ = std::make_unique<WebAppInstallManager>(profile);
  manifest_update_manager_ = std::make_unique<ManifestUpdateManager>();
  externally_managed_app_manager_ =
      std::make_unique<ExternallyManagedAppManager>(profile);
  preinstalled_web_app_manager_ =
      std::make_unique<PreinstalledWebAppManager>(profile);
  web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(profile);
  iwa_command_line_install_manager_ =
      std::make_unique<IsolatedWebAppCommandLineInstallManager>(*profile);

  database_factory_ = std::make_unique<WebAppDatabaseFactory>(profile);

  std::unique_ptr<WebAppRegistrar> registrar;
  std::unique_ptr<WebAppSyncBridge> sync_bridge;

  // Only WebAppSyncBridge must have an access to mutable WebAppRegistrar.
  {
    auto mutable_registrar = std::make_unique<WebAppRegistrarMutable>(profile);

    sync_bridge = std::make_unique<WebAppSyncBridge>(mutable_registrar.get());

    // Upcast to read-only WebAppRegistrar.
    registrar = std::move(mutable_registrar);
  }

  icon_manager_ = std::make_unique<WebAppIconManager>(
      profile, base::MakeRefCounted<FileUtilsWrapper>());
  translation_manager_ = std::make_unique<WebAppTranslationManager>(
      profile, base::MakeRefCounted<FileUtilsWrapper>());
  install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(profile);

  if (g_os_integration_manager_factory_for_testing) {
    os_integration_manager_ =
        g_os_integration_manager_factory_for_testing(profile);
  } else {
    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile);
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile);
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile, icon_manager_.get(), file_handler_manager.get(),
        protocol_handler_manager.get());

    // TODO(crbug.com/1072058): Remove UrlHandlerManager from
    // OsIntegrationManager.
    os_integration_manager_ = std::make_unique<OsIntegrationManager>(
        profile, std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager), /*url_handler_manager=*/nullptr);
  }

  command_manager_ = std::make_unique<WebAppCommandManager>(profile, this);
  command_scheduler_ = std::make_unique<WebAppCommandScheduler>(*profile, this);

  origin_association_manager_ =
      std::make_unique<WebAppOriginAssociationManager>();

  registrar_ = std::move(registrar);
  sync_bridge_ = std::move(sync_bridge);

#if (BUILDFLAG(IS_CHROMEOS))
  web_app_run_on_os_login_manager_ =
      std::make_unique<WebAppRunOnOsLoginManager>(command_scheduler_.get());
#endif
}

void WebAppProvider::ConnectSubsystems() {
  DCHECK(!started_);

  sync_bridge_->SetSubsystems(database_factory_.get(), command_manager_.get(),
                              command_scheduler_.get(), install_manager_.get());
  icon_manager_->SetSubsystems(registrar_.get(), install_manager_.get());
  install_finalizer_->SetSubsystems(
      install_manager_.get(), registrar_.get(), ui_manager_.get(),
      sync_bridge_.get(), os_integration_manager_.get(), icon_manager_.get(),
      web_app_policy_manager_.get(), translation_manager_.get(),
      command_manager_.get(), origin_association_manager_.get());
  manifest_update_manager_->SetSubsystems(install_manager_.get(),
                                          registrar_.get(), ui_manager_.get(),
                                          command_scheduler_.get());
  externally_managed_app_manager_->SetSubsystems(
      ui_manager_.get(), install_finalizer_.get(), command_scheduler_.get());
  preinstalled_web_app_manager_->SetSubsystems(
      registrar_.get(), ui_manager_.get(),
      externally_managed_app_manager_.get());
  web_app_policy_manager_->SetSubsystems(externally_managed_app_manager_.get(),
                                         registrar_.get(), sync_bridge_.get(),
                                         os_integration_manager_.get());
  registrar_->SetSubsystems(web_app_policy_manager_.get(),
                            translation_manager_.get());
  ui_manager_->SetSubsystems(sync_bridge_.get(), os_integration_manager_.get());
  os_integration_manager_->SetSubsystems(sync_bridge_.get(), registrar_.get(),
                                         ui_manager_.get(),
                                         icon_manager_.get());
  iwa_command_line_install_manager_->SetSubsystems(command_scheduler_.get());
  connected_ = true;
}

void WebAppProvider::StartSyncBridge() {
  sync_bridge_->Init(base::BindOnce(&WebAppProvider::OnSyncBridgeReady,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void WebAppProvider::OnSyncBridgeReady() {
  DCHECK(!on_registry_ready_.is_signaled());

  if (base::FeatureList::IsEnabled(features::kMigrateExternalPrefsToWebAppDB)) {
    ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
        profile_->GetPrefs(), sync_bridge_.get());
  }
  DoMigrateProfilePrefs(profile_);

  // Note: This does not wait for the call from the ChromeOS
  // SystemWebAppManager, which is a separate keyed service.
  int num_barrier_calls = 2;
  base::RepeatingClosure external_manager_barrier = base::BarrierClosure(
      num_barrier_calls,
      base::BindOnce(
          [](base::WeakPtr<WebAppProvider> provider) {
            if (!provider)
              return;
            provider->on_external_managers_synchronized_.Signal();
          },
          weak_ptr_factory_.GetWeakPtr()));

  registrar_->Start();
  install_finalizer_->Start();
  icon_manager_->Start();
  translation_manager_->Start();
  install_manager_->Start();
  preinstalled_web_app_manager_->Start(external_manager_barrier);
  web_app_policy_manager_->Start(external_manager_barrier);
  iwa_command_line_install_manager_->Start();

#if (BUILDFLAG(IS_CHROMEOS))
  on_external_managers_synchronized_.Post(
      FROM_HERE,
      base::BindOnce(&WebAppRunOnOsLoginManager::Start,
                     web_app_run_on_os_login_manager_->GetWeakPtr()));
#endif
  manifest_update_manager_->Start();
  os_integration_manager_->Start();
  ui_manager_->Start();
  command_manager_->Start();

  on_registry_ready_.Signal();
  is_registry_ready_ = true;
}

void WebAppProvider::CheckIsConnected() const {
  DCHECK(connected_) << "Attempted to access Web App subsystem while "
                        "WebAppProvider is not connected. You may need to wait "
                        "for on_registry_ready().";
}

void WebAppProvider::DoMigrateProfilePrefs(Profile* profile) {
  std::map<AppId, int> sources =
      TakeAllWebAppInstallSources(profile->GetPrefs());
  ScopedRegistryUpdate update(sync_bridge_.get());
  for (const auto& iter : sources) {
    WebApp* web_app = update->UpdateApp(iter.first);
    if (web_app && !web_app->latest_install_source()) {
      web_app->SetLatestInstallSource(
          static_cast<webapps::WebappInstallSource>(iter.second));
    }
  }
}

}  // namespace web_app
