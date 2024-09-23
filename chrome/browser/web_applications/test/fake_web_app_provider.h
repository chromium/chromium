// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROVIDER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace web_app {

class AbstractWebAppDatabaseFactory;
class ExternallyManagedAppManager;
class FileUtilsWrapper;
class IsolatedWebAppInstallationManager;
class IsolatedWebAppUpdateManager;
class OsIntegrationManager;
class PreinstalledWebAppManager;
class WebAppCommandManager;
class WebAppCommandScheduler;
class WebAppIconManager;
class WebAppInstallFinalizer;
class WebAppInstallManager;
class WebAppOriginAssociationManager;
class WebAppPolicyManager;
class WebAppRegistrarMutable;
class WebAppSyncBridge;
class WebAppTranslationManager;
class WebAppUiManager;
class WebContentsManager;

#if BUILDFLAG(IS_CHROMEOS)
class WebAppRunOnOsLoginManager;
#endif

// This is a tool that allows unit tests (enabled by default) and browser tests
// (disabled by default) to use a 'fake' version of the WebAppProvider system.
// This means that most of the dependencies are faked out. Specifically:
// * The database is in-memory.
// * Integration with Chrome Sync is off.
// * OS integration (saving shortcuts on disk, etc) doesn't execute, but does
//   save its 'expected' state to the database.
// * All access to `WebContents` is redirected to the `FakeWebContentsManager`
//   (accessible via `GetFakeWebContentsManager()`), which stores & returns
//   results for any interaction here.
//
// Other features & notes:
// * FakeWebAppProvider is used by default in unit tests, as the
//   `TestingProfile` hardcodes the usage of this fake version in the
//   `KeyedServiceFactory` for the profile,
// * The system in NOT 'started' by default, which means commands and other
//   operations will not run. This allows tests to do any customization they may
//   want by calling `FakeWebAppProvider::Get()` and interacting with this
//   object before the system is started.
//   * Generally the system is then started by calling
//     `web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());`, which
//     waits for the system to be `ready()`, but also for any external install
//     managers to finish startup as well.
// * To use this in browser tests, you must use the FakeWebAppProviderCreator
//   below.
//
// Future improvements:
// * TODO(http://b/257529391): Isolate the extensions system dependency to
//   allow faking that as well.
// * TODO(http://b/279198384): Make better default for external manager behavior
//   in unittests. Perhaps don't start them by default unless they specified.
// * TODO(http://b/279198562): Create a `FakeWebAppCommandScheduler`, allowing
//   external systems to fully fake out usage of this system if they desire.
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

  // |run_system_on_start| is false by default, and must be set to true here
  // BEFORE WebAppProvider::Start is called to allow the system to start
  // normally. Otherwise, StartWithSubsystems must be called.
  void SetStartSystemOnStart(bool run_system_on_start);

  // The PreinstalledWebAppManager waits for some dependencies (extensions and
  // device initialization) on startup, and then processes the preinstalled apps
  // configurion to install (or uninstall) apps on the profile. This is disabled
  // by default for unit tests, and can be enabled by setting this flag to true.
  void SetSynchronizePreinstalledAppsOnStartup(bool synchronize_on_startup);

  // Call when the unit tets wants to trigger OS integration, removing the
  // ScopedSuppressOsHooks in FakeOsIntegrationManager (allowing the
  // OsIntegrationTestOverrideBlockingRegistration to work correctly).
  void UseRealOsIntegrationManager();

  enum class AutomaticIwaUpdateStrategy {
    kDefault,
    kForceDisabled,
    kForceEnabled,
  };

  // The `IsolatedWebAppUpdateManager` will check for updates of all installed
  // Isolated Web Apps on startup and in regular time intervals. This is
  // disabled (`kForceDisabled`) by default for unit tests, and can be enabled
  // by setting this flag to `kForceEnabled`. Setting this flag to `kDefault`
  // will retain the default behavior of the `IsolatedWebAppUpdateManager`.
  void SetEnableAutomaticIwaUpdates(
      AutomaticIwaUpdateStrategy automatic_iwa_update_strategy);

  // NB: If you replace the Registrar, you also have to replace the SyncBridge
  // accordingly.
  void SetRegistrar(std::unique_ptr<WebAppRegistrarMutable> registrar);
  void SetDatabaseFactory(
      std::unique_ptr<AbstractWebAppDatabaseFactory> database_factory);
  void SetSyncBridge(std::unique_ptr<WebAppSyncBridge> sync_bridge);
  void SetFileUtils(scoped_refptr<FileUtilsWrapper> file_utils);
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
  void SetIsolatedWebAppInstallationManager(
      std::unique_ptr<IsolatedWebAppInstallationManager>
          isolated_web_app_installation_manager);
  void SetIsolatedWebAppUpdateManager(
      std::unique_ptr<IsolatedWebAppUpdateManager> iwa_update_manager);
#if BUILDFLAG(IS_CHROMEOS)
  void SetWebAppRunOnOsLoginManager(std::unique_ptr<WebAppRunOnOsLoginManager>
                                        web_app_run_on_os_login_manager);
#endif
  void SetCommandManager(std::unique_ptr<WebAppCommandManager> command_manager);
  void SetScheduler(std::unique_ptr<WebAppCommandScheduler> scheduler);
  void SetPreinstalledWebAppManager(
      std::unique_ptr<PreinstalledWebAppManager> preinstalled_web_app_manager);
  void SetOriginAssociationManager(
      std::unique_ptr<WebAppOriginAssociationManager>
          origin_association_manager);
  void SetWebContentsManager(
      std::unique_ptr<WebContentsManager> web_contents_manager);

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

  // Starts this WebAppProvider and its subsystems. It does not wait for systems
  // to be ready.
  void StartWithSubsystems();

  // Create and set default fake subsystems.
  void CreateFakeSubsystems();

  // Used to verify shutting down of WebAppUiManager.
  void ShutDownUiManagerForTesting();

  // Shut down subsystems one by one if they are still running.
  // This needs to be done because of functions like
  // ShutDownUiManagerForTesting() which can shut down
  // specific subsystems from tests, and still call
  // FakeWebAppProvider::Shutdown() as part of test teardown.
  void Shutdown() override;

  FakeWebAppProvider* AsFakeWebAppProviderForTesting() override;

  syncer::MockDataTypeLocalChangeProcessor& processor() {
    return mock_processor_;
  }

 private:
  // CHECK that `Start()` has not been called on this provider, and also
  // disconnect so that clients are forced to call `Start()` before accessing
  // any subsystems.
  void CheckNotStartedAndDisconnect(std::string optional_message = "");

  // WebAppProvider:
  void StartImpl() override;

  // If true, when Start()ed the FakeWebAppProvider will call
  // WebAppProvider::StartImpl() and fire startup tasks like a real
  // WebAppProvider.
  bool run_system_on_start_ = false;
  // If true, preinstalled apps will be processed & installed (or uninstalled)
  // after the system starts.
  bool synchronize_preinstalled_app_on_startup_ = false;
  // If `kForceEnabled`, the `IsolatedWebAppUpdateManager` will automatically
  // search for updates of installed Isolated Web Apps on startup and in regular
  // time intervals. If `kForceDisabled`, then it will not automatically search
  // for updates. If `kDefault`, then it will use its default behavior to
  // determine whether to search for updates (e.g., based feature flags).
  AutomaticIwaUpdateStrategy automatic_iwa_update_strategy_ =
      AutomaticIwaUpdateStrategy::kForceDisabled;

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
};

// Used in BrowserTests to ensure that the WebAppProvider that is create on
// profile startup is the FakeWebAppProvider. Hooks into the
// BrowserContextKeyedService initialization pipeline.
class FakeWebAppProviderCreator {
 public:
  using CreateWebAppProviderCallback =
      base::RepeatingCallback<std::unique_ptr<KeyedService>(Profile* profile)>;

  // Uses FakeWebAppProvider::BuildDefault to build the FakeWebAppProvider.
  FakeWebAppProviderCreator();
  // Uses the given callback to create the FakeWebAppProvider.
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
