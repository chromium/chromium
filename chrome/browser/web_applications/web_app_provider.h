// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
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

class AbstractWebAppDatabaseFactory;
class WebAppSyncBridge;
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
class WebAppTranslationManager;
class WebAppCommandManager;

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
  static WebAppProvider* GetDeprecated(Profile* profile);

  // On Chrome OS: if Lacros Web App (WebAppsCrosapi) is enabled, returns
  // WebAppProvider in Lacros and nullptr in Ash. Otherwise does the reverse
  // (nullptr in Lacros, WebAppProvider in Ash). On other platforms, always
  // returns a WebAppProvider.
  static WebAppProvider* GetForWebApps(Profile* profile);

  // On Chrome OS: returns the WebAppProvider that hosts System Web Apps in Ash;
  // In Lacros, returns nullptr (unless EnableSystemWebAppInLacrosForTesting).
  // On other platforms, always returns a WebAppProvider.
  static WebAppProvider* GetForSystemWebApps(Profile* profile);

  // Return the WebAppProvider for the current process. In particular:
  // In Ash: Returns the WebAppProvider that hosts System Web Apps.
  // In Lacros and other platforms: Returns the WebAppProvider that hosts
  // non-system Web Apps.
  //
  // Avoid using this function where possible and prefer GetForWebApps or
  // GetForSystemWebApps which provide a guarantee they are being called from
  // the correct process. Only use this if the calling code is shared between
  // Ash and Lacros and expects the PWA WebAppProvider in Lacros and the SWA
  // WebAppProvider in Ash.
  static WebAppProvider* GetForLocalAppsUnchecked(Profile* profile);

  // Return the WebAppProvider for tests, regardless of whether this is running
  // in Lacros/Ash. Blocks if the web app registry is not yet ready.
  static WebAppProvider* GetForTest(Profile* profile);

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
  const WebAppRegistrar& registrar() const;
  // The app registry controller.
  WebAppSyncBridge& sync_bridge();
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

  WebAppTranslationManager& translation_manager();

  SystemWebAppManager& system_web_app_manager();

  // Manage all OS hooks that need to be deployed during Web Apps install
  OsIntegrationManager& os_integration_manager();
  const OsIntegrationManager& os_integration_manager() const;

  WebAppCommandManager& command_manager();

  // KeyedService:
  void Shutdown() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Kicks off a migration of some entries from the `web_app_ids` pref
  // dictionary to the web app database. This should be safe to delete one year
  // after 02-2022.
  static void MigrateProfilePrefs(Profile* profile);

  // Signals when app registry becomes ready.
  const base::OneShotEvent& on_registry_ready() const {
    return on_registry_ready_;
  }

  // Returns whether the app registry is ready.
  bool is_registry_ready() const { return is_registry_ready_; }

  PreinstalledWebAppManager& preinstalled_web_app_manager() {
    return *preinstalled_web_app_manager_;
  }

 protected:
  virtual void StartImpl();
  void WaitForExtensionSystemReady();
  void OnExtensionSystemReady();

  void CreateSubsystems(Profile* profile);

  // Wire together subsystems but do not start them (yet).
  void ConnectSubsystems();

  // Start sync bridge. All other subsystems depend on it.
  void StartSyncBridge();
  void OnSyncBridgeReady();

  void CheckIsConnected() const;

  void DoMigrateProfilePrefs(Profile* profile);

  std::unique_ptr<AbstractWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppRegistrar> registrar_;
  std::unique_ptr<WebAppSyncBridge> sync_bridge_;
  std::unique_ptr<PreinstalledWebAppManager> preinstalled_web_app_manager_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppTranslationManager> translation_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<ManifestUpdateManager> manifest_update_manager_;
  std::unique_ptr<ExternallyManagedAppManager> externally_managed_app_manager_;
  std::unique_ptr<SystemWebAppManager> system_web_app_manager_;
  std::unique_ptr<WebAppAudioFocusIdMap> audio_focus_id_map_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;
  std::unique_ptr<WebAppUiManager> ui_manager_;
  std::unique_ptr<OsIntegrationManager> os_integration_manager_;
  std::unique_ptr<WebAppCommandManager> command_manager_;

  base::OneShotEvent on_registry_ready_;

  const raw_ptr<Profile> profile_;

  // Ensures that ConnectSubsystems() is not called after Start().
  bool started_ = false;
  bool connected_ = false;
  bool is_registry_ready_ = false;

  bool skip_awaiting_extension_system_ = false;

  base::WeakPtrFactory<WebAppProvider> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
