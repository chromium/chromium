// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace web_app {

// Forward declarations of generalized interfaces.
class PendingAppManager;
class InstallManager;
class InstallFinalizer;
class AppRegistrar;
class AppRegistryController;
class FileHandlerManager;
class AppIconManager;
class AppShortcutManager;
class WebAppPolicyManager;
class ManifestUpdateManager;
class WebAppAudioFocusIdMap;
class WebAppUiManager;

class WebAppProviderBase : public KeyedService {
 public:
  static WebAppProviderBase* GetProviderBase(Profile* profile);

  WebAppProviderBase();
  ~WebAppProviderBase() override;

  // The app registry model.
  virtual AppRegistrar& registrar() = 0;
  // The app registry controller.
  virtual AppRegistryController& registry_controller() = 0;
  // UIs can use InstallManager for user-initiated Web Apps install.
  virtual InstallManager& install_manager() = 0;
  // Implements persistence for Web Apps install.
  virtual InstallFinalizer& install_finalizer() = 0;
  // Keeps app metadata up to date with site manifests.
  virtual ManifestUpdateManager& manifest_update_manager() = 0;
  // Clients can use PendingAppManager to install, uninstall, and update
  // Web Apps.
  virtual PendingAppManager& pending_app_manager() = 0;
  // Clients can use WebAppPolicyManager to request updates of policy installed
  // Web Apps.
  virtual WebAppPolicyManager& policy_manager() = 0;

  virtual WebAppUiManager& ui_manager() = 0;

  virtual WebAppAudioFocusIdMap& audio_focus_id_map() = 0;

  virtual FileHandlerManager& file_handler_manager() = 0;

  // Implements fetching of app icons.
  virtual AppIconManager& icon_manager() = 0;

  virtual AppShortcutManager& shortcut_manager() = 0;

  DISALLOW_COPY_AND_ASSIGN(WebAppProviderBase);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_H_
