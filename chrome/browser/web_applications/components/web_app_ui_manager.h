// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_UI_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_UI_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class AppRegistryController;
// WebAppUiManagerImpl can be used only in UI code.
class WebAppUiManagerImpl;

// Pure virtual interface used to perform Web App UI operations or listen to Web
// App UI events.
class WebAppUiManager {
 public:
  static std::unique_ptr<WebAppUiManager> Create(Profile* profile);

  static bool ShouldHideAppFromUser(const AppId& app_id);

  virtual ~WebAppUiManager() = default;

  virtual void SetSubsystems(
      AppRegistryController* app_registry_controller) = 0;
  virtual void Start() = 0;
  virtual void Shutdown() = 0;

  // A safe downcast.
  virtual WebAppUiManagerImpl* AsImpl() = 0;

  virtual size_t GetNumWindowsForApp(const AppId& app_id) = 0;

  virtual void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                           base::OnceClosure callback) = 0;

  // Uninstalls the the apps in |from_apps| and migrates an |to_app|'s OS
  // attributes (e.g pin position, app list folder/position, shortcuts) to the
  // first |from_app| found.
  virtual void UninstallAndReplaceIfExists(const std::vector<AppId>& from_apps,
                                           const AppId& to_app) = 0;

  virtual bool CanAddAppToQuickLaunchBar() const = 0;
  virtual void AddAppToQuickLaunchBar(const AppId& app_id) = 0;

  // Returns whether |web_contents| is in a web app window belonging to
  // |app_id|, or any web app window if |app_id| is nullptr.
  virtual bool IsInAppWindow(content::WebContents* web_contents,
                             const AppId* app_id = nullptr) const = 0;
  virtual void NotifyOnAssociatedAppChanged(content::WebContents* web_contents,
                                            const AppId& previous_app_id,
                                            const AppId& new_app_id) const = 0;

  virtual bool CanReparentAppTabToWindow(const AppId& app_id,
                                         bool shortcut_created) const = 0;
  virtual void ReparentAppTabToWindow(content::WebContents* contents,
                                      const AppId& app_id,
                                      bool shortcut_created) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_UI_MANAGER_H_
