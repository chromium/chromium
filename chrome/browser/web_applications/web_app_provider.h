// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

// Forward declarations of generalized interfaces.
class AppRegistryController;
class WebAppIconManager;
class PreinstalledWebAppManager;
class WebAppInstallFinalizer;
class ManifestUpdateManager;
class SystemWebAppManager;
class WebAppAudioFocusIdMap;
class WebAppInstallManager;
class WebAppPolicyManager;
class WebAppUiManager;
class OsIntegrationManager;

// Forward declarations for new extension-independent subsystems.
class WebAppDatabaseFactory;
class WebAppMover;

// Connects Web App features, such as the installation of default and
// policy-managed web apps, with Profiles (as WebAppProvider is a
// Profile-linked KeyedService) and their associated PrefService.
//
// Lifecycle notes:
// All subsystems are constructed independently of each other in the
// WebAppProvider constructor.
// Subsystem construction should have no side effects and start no tasks.
// Tests can replace any of the subsystems before Start() is called.
// Similarly, in destruction, subsystems should not refer to each other.
class WebAppProvider : public KeyedService {
 public:
  // Deprecated: Use GetForWebApps or GetForSystemWebApps instead.
  static WebAppProvider* Get(Profile* profile);

  // On Chrome OS: if Lacros Web App (WebAppsCrosapi) is enabled, returns
  // WebAppProvider in Lacros and nullptr in Ash. Otherwise does the reverse
  // (nullptr in Lacros, WebAppProvider in Ash). On other platforms, always
  // returns a WebAppProvider.
  static WebAppProvider* GetForWebApps(Profile* profile);

  // On Chrome OS: returns the WebAppProvider that hosts System Web Apps in Ash;
  // In Lacros, returns nullptr (unless EnableSystemWebAppInLacrosForTesting).
  // On other platforms, always returns a WebAppProvider.
  static WebAppProvider* GetForSystemWebApps(Profile* profile);

  // Always returns a WebAppProvider.
  // In Ash: Returns the WebAppProvider that hosts System Web Apps.
  // In Lacros: Returns the WebAppProvider that hosts non-system Web Apps.
  // This function should only be used in code that is shared between system and
  // non-system Web Apps.
  static WebAppProvider* GetForLocalApps(Profile* profile);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Enables System Web Apps WebAppProvider so we can test SWA features in
  // Lacros, even we don't have actual SWAs in Lacros. After calling this,
  // GetForSystemWebApps will return a valid WebAppProvider in Lacros.
  static void EnableSystemWebAppsInLacrosForTesting();
#endif

  static WebAppProvider* GetForWebContents(content::WebContents* web_contents);

  using OsIntegrationManagerFactory =
      std::unique_ptr<OsIntegrationManager> (*)(Profile*);
  static void SetOsIntegrationManagerFactoryForTesting(
      OsIntegrationManagerFactory factory);

  explicit WebAppProvider(Profile* profile);
  WebAppProvider(const WebAppProvider&) = delete;
  WebAppProvider& operator=(const WebAppProvider&) = delete;
  ~WebAppProvider() override;

  // Start the Web App system. This will run subsystem startup tasks.
  void Start();

  // The app registry model.
  WebAppRegistrar& registrar();
  // The app registry controller.
  AppRegistryController& registry_controller();
  // UIs can use WebAppInstallManager for user-initiated Web Apps install.
  WebAppInstallManager& install_manager();
  // Implements persistence for Web Apps install.
  WebAppInstallFinalizer& install_finalizer();
  // Keeps app metadata up to date with site manifests.
  ManifestUpdateManager& manifest_update_manager();
  // Clients can use ExternallyManagedAppManager to install, uninstall, and
  // update Web Apps.
  ExternallyManagedAppManager& externally_managed_app_manager();
  // Clients can use WebAppPolicyManager to request updates of policy installed
  // Web Apps.
  WebAppPolicyManager& policy_manager();

  WebAppUiManager& ui_manager();

  WebAppAudioFocusIdMap& audio_focus_id_map();

  // Implements fetching of app icons.
  WebAppIconManager& icon_manager();

  SystemWebAppManager& system_web_app_manager();

  // Manage all OS hooks that need to be deployed during Web Apps install
  OsIntegrationManager& os_integration_manager();

  // KeyedService:
  void Shutdown() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Signals when app registry becomes ready.
  const base::OneShotEvent& on_registry_ready() const {
    return on_registry_ready_;
  }

  PreinstalledWebAppManager& preinstalled_web_app_manager() {
    return *preinstalled_web_app_manager_;
  }

 protected:
  virtual void StartImpl();
  void WaitForExtensionSystemReady();
  void OnExtensionSystemReady();

  // Create subsystems that work with either BMO and Extension backends.
  void CreateCommonSubsystems(Profile* profile);
  // Create extension-independent subsystems.
  void CreateWebAppsSubsystems(Profile* profile);

  // Wire together subsystems but do not start them (yet).
  void ConnectSubsystems();

  // Start registry controller. All other subsystems depend on it.
  void StartRegistryController();
  void OnRegistryControllerReady();

  void CheckIsConnected() const;

  // New extension-independent subsystems:
  std::unique_ptr<WebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppMover> web_app_mover_;

  // Generalized subsystems:
  std::unique_ptr<WebAppRegistrar> registrar_;
  std::unique_ptr<AppRegistryController> registry_controller_;
  std::unique_ptr<PreinstalledWebAppManager> preinstalled_web_app_manager_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<ManifestUpdateManager> manifest_update_manager_;
  std::unique_ptr<ExternallyManagedAppManager> externally_managed_app_manager_;
  std::unique_ptr<SystemWebAppManager> system_web_app_manager_;
  std::unique_ptr<WebAppAudioFocusIdMap> audio_focus_id_map_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;
  std::unique_ptr<WebAppUiManager> ui_manager_;
  std::unique_ptr<OsIntegrationManager> os_integration_manager_;

  base::OneShotEvent on_registry_ready_;

  Profile* const profile_;

  // Ensures that ConnectSubsystems() is not called after Start().
  bool started_ = false;
  bool connected_ = false;

  bool skip_awaiting_extension_system_ = false;

  base::WeakPtrFactory<WebAppProvider> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
