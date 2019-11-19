// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"

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
class AppIconManager;
class AppShortcutManager;
class ExternalWebAppManager;
class FileHandlerManager;
class InstallFinalizer;
class ManifestUpdateManager;
class SystemWebAppManager;
class WebAppAudioFocusIdMap;
class WebAppInstallManager;
class WebAppPolicyManager;
class WebAppUiManager;

// Forward declarations for new extension-independent subsystems.
class WebAppDatabaseFactory;

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
class WebAppProvider : public WebAppProviderBase {
 public:
  static WebAppProvider* Get(Profile* profile);
  static WebAppProvider* GetForWebContents(content::WebContents* web_contents);

  explicit WebAppProvider(Profile* profile);
  ~WebAppProvider() override;

  // Start the Web App system. This will run subsystem startup tasks.
  void Start();

  // WebAppProviderBase:
  AppRegistrar& registrar() override;
  AppRegistryController& registry_controller() override;
  InstallManager& install_manager() override;
  InstallFinalizer& install_finalizer() override;
  ManifestUpdateManager& manifest_update_manager() override;
  PendingAppManager& pending_app_manager() override;
  WebAppPolicyManager& policy_manager() override;
  WebAppUiManager& ui_manager() override;
  WebAppAudioFocusIdMap& audio_focus_id_map() override;
  FileHandlerManager& file_handler_manager() override;
  AppIconManager& icon_manager() override;
  AppShortcutManager& shortcut_manager() override;

  SystemWebAppManager& system_web_app_manager();

  // KeyedService:
  void Shutdown() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Signals when app registry becomes ready.
  const base::OneShotEvent& on_registry_ready() const {
    return on_registry_ready_;
  }

 protected:
  virtual void StartImpl();

  // Create subsystems that work with either BMO and Extension backends.
  void CreateCommonSubsystems(Profile* profile);
  // Create extension-independent subsystems.
  void CreateWebAppsSubsystems(Profile* profile);
  // ... or create legacy extension-based subsystems.
  void CreateBookmarkAppsSubsystems(Profile* profile);

  // Wire together subsystems but do not start them (yet).
  void ConnectSubsystems();

  // Start registry controller. All other subsystems depend on it.
  void StartRegistryController();
  void OnRegistryControllerReady();

  void CheckIsConnected() const;

  // New extension-independent subsystems:
  std::unique_ptr<WebAppDatabaseFactory> database_factory_;

  // Generalized subsystems:
  std::unique_ptr<AppRegistrar> registrar_;
  std::unique_ptr<AppRegistryController> registry_controller_;
  std::unique_ptr<ExternalWebAppManager> external_web_app_manager_;
  std::unique_ptr<FileHandlerManager> file_handler_manager_;
  std::unique_ptr<AppIconManager> icon_manager_;
  std::unique_ptr<InstallFinalizer> install_finalizer_;
  std::unique_ptr<ManifestUpdateManager> manifest_update_manager_;
  std::unique_ptr<PendingAppManager> pending_app_manager_;
  std::unique_ptr<AppShortcutManager> shortcut_manager_;
  std::unique_ptr<SystemWebAppManager> system_web_app_manager_;
  std::unique_ptr<WebAppAudioFocusIdMap> audio_focus_id_map_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;
  std::unique_ptr<WebAppUiManager> ui_manager_;

  base::OneShotEvent on_registry_ready_;

  Profile* const profile_;

  // Ensures that ConnectSubsystems() is not called after Start().
  bool started_ = false;
  bool connected_ = false;

  base::WeakPtrFactory<WebAppProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppProvider);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
