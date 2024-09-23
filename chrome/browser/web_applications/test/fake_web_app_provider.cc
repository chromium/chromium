// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_provider.h"

#include <memory>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_identity_validator.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
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
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"
#endif

namespace web_app {

// static
std::unique_ptr<KeyedService> FakeWebAppProvider::BuildDefault(
    content::BrowserContext* context) {
  auto provider = std::make_unique<FakeWebAppProvider>(
      Profile::FromBrowserContext(context));

  // Do not call default production StartImpl if in TestingProfile.
  provider->SetStartSystemOnStart(false);

  // TODO(crbug.com/41464466): Consider calling `CreateFakeSubsystems` in the
  // constructor instead.
  provider->CreateFakeSubsystems();
  provider->ConnectSubsystems();

  return provider;
}

// static
FakeWebAppProvider* FakeWebAppProvider::Get(Profile* profile) {
  auto* test_provider = WebAppProvider::GetForLocalAppsUnchecked(profile)
                            ->AsFakeWebAppProviderForTesting();
  CHECK(test_provider);

  return test_provider;
}

FakeWebAppProvider::FakeWebAppProvider(Profile* profile)
    : WebAppProvider(profile) {}

FakeWebAppProvider::~FakeWebAppProvider() = default;

void FakeWebAppProvider::SetStartSystemOnStart(bool run_system_on_start) {
  CheckNotStartedAndDisconnect(
      "The system was already started, perhaps the profile has already been "
      "created & started.");
  run_system_on_start_ = run_system_on_start;
}

void FakeWebAppProvider::SetSynchronizePreinstalledAppsOnStartup(
    bool synchronize_on_startup) {
  CheckNotStartedAndDisconnect();
  synchronize_preinstalled_app_on_startup_ = synchronize_on_startup;
}

void FakeWebAppProvider::UseRealOsIntegrationManager() {
  CheckNotStartedAndDisconnect();
  auto file_handler_manager =
      std::make_unique<WebAppFileHandlerManager>(profile_);
  auto protocol_handler_manager =
      std::make_unique<WebAppProtocolHandlerManager>(profile_);

  SetOsIntegrationManager(std::make_unique<OsIntegrationManager>(
      profile_, std::move(file_handler_manager),
      std::move(protocol_handler_manager)));
}

void FakeWebAppProvider::SetEnableAutomaticIwaUpdates(
    AutomaticIwaUpdateStrategy automatic_iwa_update_strategy) {
  CheckNotStartedAndDisconnect();
  automatic_iwa_update_strategy_ = automatic_iwa_update_strategy;
}

void FakeWebAppProvider::SetRegistrar(
    std::unique_ptr<WebAppRegistrarMutable> registrar) {
  CheckNotStartedAndDisconnect();
  registrar_ = std::move(registrar);
}

void FakeWebAppProvider::SetDatabaseFactory(
    std::unique_ptr<AbstractWebAppDatabaseFactory> database_factory) {
  CheckNotStartedAndDisconnect();
  database_factory_ = std::move(database_factory);
}

void FakeWebAppProvider::SetSyncBridge(
    std::unique_ptr<WebAppSyncBridge> sync_bridge) {
  CheckNotStartedAndDisconnect();
  sync_bridge_ = std::move(sync_bridge);
}

void FakeWebAppProvider::SetFileUtils(
    scoped_refptr<FileUtilsWrapper> file_utils) {
  CheckNotStartedAndDisconnect();
  file_utils_ = file_utils;
}

void FakeWebAppProvider::SetIconManager(
    std::unique_ptr<WebAppIconManager> icon_manager) {
  CheckNotStartedAndDisconnect();
  icon_manager_ = std::move(icon_manager);
}

void FakeWebAppProvider::SetTranslationManager(
    std::unique_ptr<WebAppTranslationManager> translation_manager) {
  CheckNotStartedAndDisconnect();
  translation_manager_ = std::move(translation_manager);
}

void FakeWebAppProvider::SetOsIntegrationManager(
    std::unique_ptr<OsIntegrationManager> os_integration_manager) {
  CheckNotStartedAndDisconnect();
  os_integration_manager_ = std::move(os_integration_manager);
}

void FakeWebAppProvider::SetInstallManager(
    std::unique_ptr<WebAppInstallManager> install_manager) {
  CheckNotStartedAndDisconnect();
  install_manager_ = std::move(install_manager);
}

void FakeWebAppProvider::SetInstallFinalizer(
    std::unique_ptr<WebAppInstallFinalizer> install_finalizer) {
  CheckNotStartedAndDisconnect();
  install_finalizer_ = std::move(install_finalizer);
}

void FakeWebAppProvider::SetExternallyManagedAppManager(
    std::unique_ptr<ExternallyManagedAppManager>
        externally_managed_app_manager) {
  CheckNotStartedAndDisconnect();
  externally_managed_app_manager_ = std::move(externally_managed_app_manager);
}

void FakeWebAppProvider::SetWebAppUiManager(
    std::unique_ptr<WebAppUiManager> ui_manager) {
  CheckNotStartedAndDisconnect();
  ui_manager_ = std::move(ui_manager);
}

void FakeWebAppProvider::SetIsolatedWebAppInstallationManager(
    std::unique_ptr<IsolatedWebAppInstallationManager>
        isolated_web_app_installation_manager) {
  CheckNotStartedAndDisconnect();
  isolated_web_app_installation_manager_ =
      std::move(isolated_web_app_installation_manager);
}

void FakeWebAppProvider::SetWebAppPolicyManager(
    std::unique_ptr<WebAppPolicyManager> web_app_policy_manager) {
  CheckNotStartedAndDisconnect();
  web_app_policy_manager_ = std::move(web_app_policy_manager);
}

#if BUILDFLAG(IS_CHROMEOS)
void FakeWebAppProvider::SetIsolatedWebAppUpdateManager(
    std::unique_ptr<IsolatedWebAppUpdateManager> iwa_update_manager) {
  CheckNotStartedAndDisconnect();
  iwa_update_manager_ = std::move(iwa_update_manager);
}

void FakeWebAppProvider::SetWebAppRunOnOsLoginManager(
    std::unique_ptr<WebAppRunOnOsLoginManager>
        web_app_run_on_os_login_manager) {
  CheckNotStartedAndDisconnect();
  web_app_run_on_os_login_manager_ = std::move(web_app_run_on_os_login_manager);
}
#endif

void FakeWebAppProvider::SetCommandManager(
    std::unique_ptr<WebAppCommandManager> command_manager) {
  CheckNotStartedAndDisconnect();
  if (command_manager_)
    command_manager_->Shutdown();
  command_manager_ = std::move(command_manager);
}

void FakeWebAppProvider::SetScheduler(
    std::unique_ptr<WebAppCommandScheduler> scheduler) {
  CheckNotStartedAndDisconnect();
  command_scheduler_ = std::move(scheduler);
}

void FakeWebAppProvider::SetPreinstalledWebAppManager(
    std::unique_ptr<PreinstalledWebAppManager> preinstalled_web_app_manager) {
  CheckNotStartedAndDisconnect();
  preinstalled_web_app_manager_ = std::move(preinstalled_web_app_manager);
}

void FakeWebAppProvider::SetOriginAssociationManager(
    std::unique_ptr<WebAppOriginAssociationManager>
        origin_association_manager) {
  CheckNotStartedAndDisconnect();
  origin_association_manager_ = std::move(origin_association_manager);
}

void FakeWebAppProvider::SetWebContentsManager(
    std::unique_ptr<WebContentsManager> web_contents_manager) {
  CheckNotStartedAndDisconnect();
  web_contents_manager_ = std::move(web_contents_manager);
}

WebAppRegistrarMutable& FakeWebAppProvider::GetRegistrarMutable() const {
  DCHECK(registrar_);
  return *static_cast<WebAppRegistrarMutable*>(registrar_.get());
}

WebAppIconManager& FakeWebAppProvider::GetIconManager() const {
  DCHECK(icon_manager_);
  return *icon_manager_;
}

WebAppCommandManager& FakeWebAppProvider::GetCommandManager() const {
  DCHECK(command_manager_);
  return *command_manager_;
}

AbstractWebAppDatabaseFactory& FakeWebAppProvider::GetDatabaseFactory() const {
  DCHECK(database_factory_);
  return *database_factory_;
}

WebAppUiManager& FakeWebAppProvider::GetUiManager() const {
  DCHECK(ui_manager_);
  return *ui_manager_;
}

WebAppInstallManager& FakeWebAppProvider::GetInstallManager() const {
  DCHECK(install_manager_);
  return *install_manager_;
}

OsIntegrationManager& FakeWebAppProvider::GetOsIntegrationManager() const {
  DCHECK(os_integration_manager_);
  return *os_integration_manager_;
}

void FakeWebAppProvider::StartWithSubsystems() {
  CheckNotStartedAndDisconnect("Cannot start system if it's already started.");
  SetStartSystemOnStart(true);
  Start();
}

void FakeWebAppProvider::CreateFakeSubsystems() {
  // Disable preinstalled apps by default as they add noise and time to tests
  // that don't need them.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDefaultApps);

  SetDatabaseFactory(std::make_unique<FakeWebAppDatabaseFactory>());

  SetWebContentsManager(std::make_unique<FakeWebContentsManager>());

  SetOsIntegrationManager(std::make_unique<FakeOsIntegrationManager>(
      profile_,
      /*file_handler_manager=*/nullptr,
      /*protocol_handler_manager=*/nullptr));

  SetSyncBridge(std::make_unique<WebAppSyncBridge>(
      &GetRegistrarMutable(), processor().CreateForwardingProcessor()));

  SetFileUtils(base::MakeRefCounted<TestFileUtils>());

  SetWebAppUiManager(std::make_unique<FakeWebAppUiManager>());

  SetExternallyManagedAppManager(
      std::make_unique<FakeExternallyManagedAppManager>(profile_));

  IwaIdentityValidator::CreateSingleton();

  // Do not create real subsystems here. That will be done already by
  // WebAppProvider::CreateSubsystems in the WebAppProvider constructor.

  ON_CALL(processor(), IsTrackingMetadata())
      .WillByDefault(testing::Return(true));
}

void FakeWebAppProvider::ShutDownUiManagerForTesting() {
  ui_manager_.reset();
}

void FakeWebAppProvider::Shutdown() {
  if (command_scheduler_)
    command_scheduler_->Shutdown();
  if (command_manager_)
    command_manager_->Shutdown();
  if (ui_manager_)
    ui_manager_->Shutdown();
  if (externally_managed_app_manager_)
    externally_managed_app_manager_->Shutdown();
  if (manifest_update_manager_)
    manifest_update_manager_->Shutdown();
  if (iwa_update_manager_) {
    iwa_update_manager_->Shutdown();
  }
  if (install_manager_)
    install_manager_->Shutdown();
  web_app_policy_manager_->Shutdown();
  if (icon_manager_)
    icon_manager_->Shutdown();
  if (install_finalizer_)
    install_finalizer_->Shutdown();
  if (os_integration_manager_) {
    os_integration_manager_->Shutdown();
  }
  is_registry_ready_ = false;
}

FakeWebAppProvider* FakeWebAppProvider::AsFakeWebAppProviderForTesting() {
  return this;
}

void FakeWebAppProvider::CheckNotStartedAndDisconnect(
    std::string optional_message) {
  CHECK(!started_) << "Attempted to set a WebAppProvider subsystem after "
                      "Start() was called. "
                   << optional_message;
  connected_ = false;
}

void FakeWebAppProvider::StartImpl() {
  preinstalled_web_app_manager_->SetSkipStartupSynchronizeForTesting(
      !synchronize_preinstalled_app_on_startup_);

  switch (automatic_iwa_update_strategy_) {
    case AutomaticIwaUpdateStrategy::kDefault:
      break;
    case AutomaticIwaUpdateStrategy::kForceDisabled:
      iwa_update_manager_->SetEnableAutomaticUpdatesForTesting(false);
      break;
    case AutomaticIwaUpdateStrategy::kForceEnabled:
      iwa_update_manager_->SetEnableAutomaticUpdatesForTesting(true);
      break;
  }

  if (run_system_on_start_) {
    WebAppProvider::StartImpl();
  } else {
    on_registry_ready_.Signal();
    is_registry_ready_ = true;
  }
}

FakeWebAppProviderCreator::FakeWebAppProviderCreator()
    : callback_(base::BindRepeating([](Profile* profile) {
        return FakeWebAppProvider::BuildDefault(profile);
      })) {
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &FakeWebAppProviderCreator::OnWillCreateBrowserContextServices,
              base::Unretained(this)));
}

FakeWebAppProviderCreator::FakeWebAppProviderCreator(
    CreateWebAppProviderCallback callback)
    : callback_(std::move(callback)) {
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &FakeWebAppProviderCreator::OnWillCreateBrowserContextServices,
              base::Unretained(this)));
}

FakeWebAppProviderCreator::~FakeWebAppProviderCreator() = default;

void FakeWebAppProviderCreator::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  WebAppProviderFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(&FakeWebAppProviderCreator::CreateWebAppProvider,
                          base::Unretained(this)));
}

std::unique_ptr<KeyedService> FakeWebAppProviderCreator::CreateWebAppProvider(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!WebAppProviderFactory::IsServiceCreatedForProfile(profile));
  if (!AreWebAppsEnabled(profile) || !callback_)
    return nullptr;
  return callback_.Run(profile);
}

}  // namespace web_app
