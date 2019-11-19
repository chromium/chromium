// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/app_shortcut_observer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"

class Profile;

namespace web_app {

class FileHandlerManager : public AppRegistrarObserver,
                           public AppShortcutObserver {
 public:
  explicit FileHandlerManager(Profile* profile);
  ~FileHandlerManager() override;

  // |registrar| is used to observe OnWebAppInstalled/Uninstalled events.
  void SetSubsystems(AppRegistrar* registrar,
                     AppShortcutManager* shortcut_manager);
  void Start();

  // Gets all file handlers for |app_id|. |nullptr| if the app has no file
  // handlers.
  // Note: The lifetime of the file handlers are tied to the app they belong to.
  virtual const std::vector<apps::FileHandlerInfo>* GetFileHandlers(
      const AppId& app_id) = 0;

 protected:
  Profile* profile() { return profile_; }

 private:
  Profile* const profile_;
  AppRegistrar* registrar_ = nullptr;
  AppShortcutManager* shortcut_manager_ = nullptr;

  // AppRegistrarObserver:
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppProfileWillBeDeleted(const AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  // AppShortcutObserver:
  void OnShortcutsCreated(const AppId& app_id) override;
  void OnShortcutManagerDestroyed() override;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> registrar_observer_;
  ScopedObserver<AppShortcutManager, AppShortcutObserver> shortcut_observer_;

  DISALLOW_COPY_AND_ASSIGN(FileHandlerManager);
};

// Compute the set of file extensions specified in |file_handlers|.
std::set<std::string> GetFileExtensionsFromFileHandlers(
    const std::vector<apps::FileHandlerInfo>& file_handlers);

// Compute the set of mime types specified in |file_handlers|.
std::set<std::string> GetMimeTypesFromFileHandlers(
    const std::vector<apps::FileHandlerInfo>& file_handlers);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLER_MANAGER_H_
