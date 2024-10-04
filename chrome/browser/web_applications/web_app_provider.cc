// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_closures.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/generated_icon_fix_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/navigation_capturing_log.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/visited_manifest_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_ui_state_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/feature_list.h"
#include "chrome/browser/web_applications/ash/migrations/adobe_express_oem_to_default_migration.h"
#include "chrome/browser/web_applications/ash/migrations/migrate_preinstalls_to_aps.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

namespace webapps {
enum class WebappInstallSource;
}

namespace web_app {

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
  // An exception is that Shimless RMA app always requires loading IWA on Ash.
  // TODO(b/292227137): Migrate Shimless RMA app to LaCrOS.
  if (IsWebAppsCrosapiEnabled() &&
      (!::ash::features::IsShimlessRMA3pDiagnosticsEnabled() ||
       !::ash::IsShimlessRmaAppBrowserContext(profile))) {
    return nullptr;
  }
#endif
  return WebAppProviderFactory::GetForProfile(profile);
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

WebAppCommandScheduler& WebAppProvider::scheduler() {
  return *command_scheduler_;
}

WebAppCommandManager& WebAppProvider::command_manager() {
  // Note: It is OK to access the command manager before connection or start.
  // Internally it will queue commands to only happen after it has started.
  return *command_manager_;
}

WebAppRegistrar& WebAppProvider::registrar_unsafe() {
  CheckIsConnected();
  return *registrar_;
}

const WebAppRegistrar& WebAppProvider::registrar_unsafe() const {
  CheckIsConnected();
  return *registrar_;
}

WebAppRegistrarMutable& WebAppProvider::registrar_mutable(
    base::PassKey<WebAppSyncBridge>) {
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

IsolatedWebAppInstallationManager&
WebAppProvider::isolated_web_app_installation_manager() {
  CheckIsConnected();
  return *isolated_web_app_installation_manager_;
}

IsolatedWebAppUpdateManager& WebAppProvider::iwa_update_manager() {
  CheckIsConnected();
  return *iwa_update_manager_;
}

#if BUILDFLAG(IS_CHROMEOS)
WebAppRunOnOsLoginManager& WebAppProvider::run_on_os_login_manager() {
  CheckIsConnected();
  return *web_app_run_on_os_login_manager_;
}

IsolatedWebAppPolicyManager& WebAppProvider::iwa_policy_manager() {
  CheckIsConnected();
  return *isolated_web_app_policy_manager_;
}
#endif

WebAppUiManager& WebAppProvider::ui_manager() {
  CheckIsConnected();
  return *ui_manager_;
}

WebAppUiStateManager& WebAppProvider::ui_state_manager() {
  CheckIsConnected();
  return *ui_state_manager_;
}

WebAppAudioFocusIdMap& WebAppProvider::audio_focus_id_map() {
  CheckIsConnected();
  return *audio_focus_id_map_;
}

scoped_refptr<FileUtilsWrapper> WebAppProvider::file_utils() {
  CheckIsConnected();
  return file_utils_;
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

WebContentsManager& WebAppProvider::web_contents_manager() {
  return *web_contents_manager_;
}

PreinstalledWebAppManager& WebAppProvider::preinstalled_web_app_manager() {
  return *preinstalled_web_app_manager_;
}

ExtensionsManager& WebAppProvider::extensions_manager() {
  return *extensions_manager_;
}

GeneratedIconFixManager& WebAppProvider::generated_icon_fix_manager() {
  return *generated_icon_fix_manager_;
}

AbstractWebAppDatabaseFactory& WebAppProvider::database_factory() {
  return *database_factory_;
}

VisitedManifestManager& WebAppProvider::visited_manifest_manager() {
  CheckIsConnected();
  return *visited_manifest_manager_;
}

NavigationCapturingLog& WebAppProvider::navigation_capturing_log() {
  CheckIsConnected();
  return *navigation_capturing_log_;
}

void WebAppProvider::Shutdown() {
  command_scheduler_->Shutdown();
  command_manager_->Shutdown();
  ui_manager_->Shutdown();
  externally_managed_app_manager_->Shutdown();
  manifest_update_manager_->Shutdown();
  iwa_update_manager_->Shutdown();
  install_manager_->Shutdown();
  web_app_policy_manager_->Shutdown();
  icon_manager_->Shutdown();
  install_finalizer_->Shutdown();
  os_integration_manager_->Shutdown();
  is_registry_ready_ = false;
}

base::WeakPtr<WebAppProvider> WebAppProvider::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

FakeWebAppProvider* WebAppProvider::AsFakeWebAppProviderForTesting() {
  return nullptr;
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
  isolated_web_app_installation_manager_ =
      std::make_unique<IsolatedWebAppInstallationManager>(*profile);
  iwa_update_manager_ = std::make_unique<IsolatedWebAppUpdateManager>(*profile);
  extensions_manager_ = std::make_unique<ExtensionsManager>(profile);
  generated_icon_fix_manager_ = std::make_unique<GeneratedIconFixManager>();

  database_factory_ = std::make_unique<WebAppDatabaseFactory>(profile);

  registrar_ = std::make_unique<WebAppRegistrarMutable>(profile);
  sync_bridge_ = std::make_unique<WebAppSyncBridge>(registrar_.get());

  file_utils_ = base::MakeRefCounted<FileUtilsWrapper>();

  icon_manager_ = std::make_unique<WebAppIconManager>(profile);
  translation_manager_ = std::make_unique<WebAppTranslationManager>(profile);
  install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(profile);

  auto file_handler_manager =
      std::make_unique<WebAppFileHandlerManager>(profile);
  auto protocol_handler_manager =
      std::make_unique<WebAppProtocolHandlerManager>(profile);

  os_integration_manager_ = std::make_unique<OsIntegrationManager>(
      profile, std::move(file_handler_manager),
      std::move(protocol_handler_manager));

  command_manager_ = std::make_unique<WebAppCommandManager>(profile);
  command_scheduler_ = std::make_unique<WebAppCommandScheduler>(*profile);

  origin_association_manager_ =
      std::make_unique<WebAppOriginAssociationManager>();

#if BUILDFLAG(IS_CHROMEOS)
  web_app_run_on_os_login_manager_ =
      std::make_unique<WebAppRunOnOsLoginManager>(profile);
  isolated_web_app_policy_manager_ =
      std::make_unique<IsolatedWebAppPolicyManager>(profile);
#endif

  web_contents_manager_ = std::make_unique<WebContentsManager>();
  ui_state_manager_ = std::make_unique<WebAppUiStateManager>();
  visited_manifest_manager_ = std::make_unique<VisitedManifestManager>();
  navigation_capturing_log_ = std::make_unique<NavigationCapturingLog>();
}

void WebAppProvider::ConnectSubsystems() {
  DCHECK(!started_);

  // TODO(https://issuetracker.google.com/283816014): Replace SetSubsystems()
  // with SetProvider().
  sync_bridge_->SetSubsystems(database_factory_.get(), command_manager_.get(),
                              command_scheduler_.get(), install_manager_.get());

  base::PassKey<WebAppProvider> pass_key;
  icon_manager_->SetProvider(pass_key, *this);
  install_finalizer_->SetProvider(pass_key, *this);
  manifest_update_manager_->SetProvider(pass_key, *this);
  externally_managed_app_manager_->SetProvider(pass_key, *this);
  preinstalled_web_app_manager_->SetProvider(pass_key, *this);
  web_app_policy_manager_->SetProvider(pass_key, *this);
  registrar_->SetProvider(pass_key, *this);
  os_integration_manager_->SetProvider(pass_key, *this);
  command_manager_->SetProvider(pass_key, *this);
  command_scheduler_->SetProvider(pass_key, *this);
  isolated_web_app_installation_manager_->SetProvider(pass_key, *this);
  iwa_update_manager_->SetProvider(pass_key, *this);
#if BUILDFLAG(IS_CHROMEOS)
  web_app_run_on_os_login_manager_->SetProvider(pass_key, *this);
  isolated_web_app_policy_manager_->SetProvider(pass_key, *this);
#endif
  icon_manager_->SetProvider(pass_key, *this);
  translation_manager_->SetProvider(pass_key, *this);
  generated_icon_fix_manager_->SetProvider(pass_key, *this);

  connected_ = true;
}

void WebAppProvider::StartSyncBridge() {
  sync_bridge_->Init(
      base::BindOnce(&WebAppProvider::OnSyncBridgeReady, AsWeakPtr()));
}

void WebAppProvider::OnSyncBridgeReady() {
  DCHECK(!on_registry_ready_.is_signaled());

  // Perform database migrations once the sync bridge is ready, but before
  // starting the rest of the subsystems and notifying that the registry is
  // ready.
#if BUILDFLAG(IS_CHROMEOS)
  web_app::migrations::MigrateAdobeExpressFromOemInstallToDefault(
      sync_bridge_.get());
  if (base::FeatureList::IsEnabled(
          chromeos::features::kPreinstalledWebAppsCoreOnly)) {
    web_app::migrations::MigratePreinstallsToAps(sync_bridge_.get());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::ConcurrentClosures concurrent;

  base::OnceClosure on_web_app_policy_manager_done_callback =
#if BUILDFLAG(IS_CHROMEOS)
      base::BindOnce(&WebAppRunOnOsLoginManager::Start,
                     web_app_run_on_os_login_manager_->GetWeakPtr())
          .Then(concurrent.CreateClosure());
#else
      concurrent.CreateClosure();
#endif  // BUILDFLAG(IS_CHROMEOS)

  registrar_->Start();
  install_finalizer_->Start();
  icon_manager_->Start();
  translation_manager_->Start();
  install_manager_->Start();
  preinstalled_web_app_manager_->Start(concurrent.CreateClosure());
  web_app_policy_manager_->Start(
      std::move(on_web_app_policy_manager_done_callback));
  isolated_web_app_installation_manager_->Start();

  iwa_update_manager_->Start();
  manifest_update_manager_->Start();
  os_integration_manager_->Start();
  ui_manager_->Start();
  generated_icon_fix_manager_->Start();
  command_manager_->Start();
#if BUILDFLAG(IS_CHROMEOS)
  isolated_web_app_policy_manager_->Start(concurrent.CreateClosure());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Note: This does not wait for the call from the ChromeOS
  // SystemWebAppManager, which is a separate keyed service.
  std::move(concurrent)
      .Done(base::BindOnce(
          [](base::WeakPtr<WebAppProvider> provider) {
            if (!provider) {
              return;
            }
            provider->on_external_managers_synchronized_.Signal();
          },
          AsWeakPtr()));

  on_registry_ready_.Signal();
  is_registry_ready_ = true;
}

void WebAppProvider::CheckIsConnected() const {
  DCHECK(connected_) << "Attempted to access Web App subsystem while "
                        "WebAppProvider is not connected. You may need to wait "
                        "for on_registry_ready().";
}

}  // namespace web_app
