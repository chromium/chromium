// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

// Forward declarations of generalized interfaces.
class PendingAppManager;
class InstallManager;

// Forward declarations for new extension-independent subsystems.
class WebAppRegistrar;

// Forward declarations for legacy extension-based subsystems.
class WebAppPolicyManager;
class SystemWebAppManager;

// Connects Web App features, such as the installation of default and
// policy-managed web apps, with Profiles (as WebAppProvider is a
// Profile-linked KeyedService) and their associated PrefService.
class WebAppProvider : public KeyedService,
                       public content::NotificationObserver {
 public:
  static WebAppProvider* Get(Profile* profile);
  static WebAppProvider* GetForWebContents(
      const content::WebContents* web_contents);

  explicit WebAppProvider(Profile* profile);

  // Clients can use PendingAppManager to install, uninstall, and update
  // Web Apps.
  PendingAppManager& pending_app_manager() { return *pending_app_manager_; }

  ~WebAppProvider() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if a bookmark can be installed for a given |web_contents|.
  static bool CanInstallWebApp(const content::WebContents* web_contents);

  // Starts a bookmark installation process for a given |web_contents|.
  static void InstallWebApp(content::WebContents* web_contents,
                            bool force_shortcut_app);

  void Reset();

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // Create extension-independent subsystems.
  void CreateWebAppsSubsystems(Profile* profile);
  // ... or create legacy extension-based subsystems.
  void CreateBookmarkAppsSubsystems(Profile* profile);

  void OnScanForExternalWebApps(
      std::vector<web_app::PendingAppManager::AppInfo>);

  // New extension-independent subsystems:
  std::unique_ptr<WebAppRegistrar> registrar_;

  // New generalized subsystems:
  std::unique_ptr<InstallManager> install_manager_;
  std::unique_ptr<PendingAppManager> pending_app_manager_;

  // Legacy extension-based subsystems:
  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;
  std::unique_ptr<SystemWebAppManager> system_web_app_manager_;

  content::NotificationRegistrar notification_registrar_;

  base::WeakPtrFactory<WebAppProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppProvider);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
