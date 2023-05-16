// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROVIDER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"
#endif

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace web_app {

class AbstractWebAppDatabaseFactory;
class WebAppRegistrar;
class OsIntegrationManager;
class WebAppInstallFinalizer;
class ExternallyManagedAppManager;
class WebAppInstallManager;
class WebAppPolicyManager;
class WebAppIconManager;
class WebAppTranslationManager;
class WebAppRegistrarMutable;
class WebAppSyncBridge;
class WebAppUiManager;
class WebAppCommandManager;
class PreinstalledWebAppManager;

class FakeWebAppProvider : public WebAppProvider {
 public:
  // Used by the TestingProfile in unit tests.
  // Builds a stub WebAppProvider which will not fire subsystem startup tasks.
  // Use FakeWebAppProvider::Get() to replace subsystems.
  static std::unique_ptr<KeyedService> BuildDefault(
      content::BrowserContext* context);

  // Gets a FakeWebAppProvider that can have its subsystems set. This should
  // only be called once during SetUp(), and clients must call Start() before
  // using the subsystems.
  static FakeWebAppProvider* Get(Profile* profile);

  explicit FakeWebAppProvider(Profile* profile);
  ~FakeWebAppProvider() override;

  // |run_subsystem_startup_tasks| is true by default as browser test clients
  // will generally want to construct their FakeWebAppProvider to behave as it
  // would in a production browser.
  //
  // |run_subsystem_startup_tasks| is false by default for FakeWebAppProvider
  // if it's a part of TestingProfile (see BuildDefault() method above).
  void SetRunSubsystemStartupTasks(bool run_subsystem_startup_tasks);

  // The PreinstalledWebAppManager waits for some dependencies (extensions and
  // device initialization) on startup, and then processes the preinstalled apps
  // configurion to install (or uninstall) apps on the profile. This is disabled
  // by default for unit tests, and can be enabled by setting this flag to true.
  void SetSynchronizePreinstalledAppsOnStartup(bool synchronize_on_startup);

  // NB: If you replace the Registrar, you also have to replace the SyncBridge
  // accordingly.
  void SetRegistrar(std::unique_ptr<WebAppRegistrar> registrar);
  void SetDatabaseFactory(
      std::unique_ptr<AbstractWebAppDatabaseFactory> database_factory);
  void SetSyncBridge(std::unique_ptr<WebAppSyncBridge> sync_bridge);
  void SetIconManager(std::unique_ptr<WebAppIconManager> icon_manager);
  void SetTranslationManager(
      std::unique_ptr<WebAppTranslationManager> translation_manager);
  void SetOsIntegrationManager(
      std::unique_ptr<OsIntegrationManager> os_integration_manager);
  void SetInstallManager(std::unique_ptr<WebAppInstallManager> install_manager);
  void SetInstallFinalizer(
      std::unique_ptr<WebAppInstallFinalizer> install_finalizer);
  void SetExternallyManagedAppManager(
      std::unique_ptr<ExternallyManagedAppManager>
          externally_managed_app_manager);
  void SetWebAppUiManager(std::unique_ptr<WebAppUiManager> ui_manager);
  void SetWebAppPolicyManager(
      std::unique_ptr<WebAppPolicyManager> web_app_policy_manager);
#if BUILDFLAG(IS_CHROMEOS)
  void SetWebAppRunOnOsLoginManager(std::unique_ptr<WebAppRunOnOsLoginManager>
                                        web_app_run_on_os_login_manager);
#endif
  void SetCommandManager(std::unique_ptr<WebAppCommandManager> command_manager);
  void SetPreinstalledWebAppManager(
      std::unique_ptr<PreinstalledWebAppManager> preinstalled_web_app_manager);
  void SetOriginAssociationManager(
      std::unique_ptr<WebAppOriginAssociationManager>
          origin_association_manager);

  // These getters can be called at any time: no
  // WebAppProvider::CheckIsConnected() check performed. See
  // WebAppProvider::ConnectSubsystems().
  //
  // A mutable view must be accessible only in tests.
  WebAppRegistrarMutable& GetRegistrarMutable() const;
  WebAppIconManager& GetIconManager() const;
  WebAppCommandManager& GetCommandManager() const;
  AbstractWebAppDatabaseFactory& GetDatabaseFactory() const;
  WebAppUiManager& GetUiManager() const;
  WebAppInstallManager& GetInstallManager() const;
  OsIntegrationManager& GetOsIntegrationManager() const;
#if BUILDFLAG(IS_CHROMEOS)
  WebAppRunOnOsLoginManager& GetWebAppRunOnOsLoginManager() const;
#endif

  // Starts this WebAppProvider and its subsystems. It does not wait for systems
  // to be ready.
  void StartWithSubsystems();

  // Create and set default fake subsystems.
  void SetDefaultFakeSubsystems();

  // Used to verify shutting down of WebAppUiManager.
  void ShutDownUiManagerForTesting();

  // Shut down subsystems one by one if they are still running.
  // This needs to be done because of functions like
  // ShutDownUiManagerForTesting() which can shut down
  // specific subsystems from tests, and still call
  // FakeWebAppProvider::Shutdown() as part of test teardown.
  void Shutdown() override;

  syncer::MockModelTypeChangeProcessor& processor() { return mock_processor_; }

 private:
  // CHECK that `Start()` has not been called on this provider, and also
  // disconnect so that clients are forced to call `Start()` before accessing
  // any subsystems.
  void CheckNotStartedAndDisconnect();

  // WebAppProvider:
  void StartImpl() override;

  // If true, when Start()ed the FakeWebAppProvider will call
  // WebAppProvider::StartImpl() and fire startup tasks like a real
  // WebAppProvider.
  bool run_subsystem_startup_tasks_ = true;
  // If true, preinstalled apps will be processed & installed (or uninstalled)
  // after the system starts.
  bool synchronize_preinstalled_app_on_startup_ = false;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
};

// Used in BrowserTests to ensure that the WebAppProvider that is create on
// profile startup is the FakeWebAppProvider. Hooks into the
// BrowserContextKeyedService initialization pipeline.
class FakeWebAppProviderCreator {
 public:
  using CreateWebAppProviderCallback =
      base::RepeatingCallback<std::unique_ptr<KeyedService>(Profile* profile)>;

  explicit FakeWebAppProviderCreator(CreateWebAppProviderCallback callback);
  ~FakeWebAppProviderCreator();

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  std::unique_ptr<KeyedService> CreateWebAppProvider(
      content::BrowserContext* context);

  CreateWebAppProviderCallback callback_;

  base::CallbackListSubscription create_services_subscription_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROVIDER_H_
