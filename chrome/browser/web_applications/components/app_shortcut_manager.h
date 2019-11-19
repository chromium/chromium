// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

class Profile;

namespace web_app {

class AppRegistrar;
class AppShortcutObserver;
struct ShortcutInfo;

// TODO(crbug.com/860581): Migrate functions from
// web_app_extension_shortcut.(h|cc) and
// platform_apps/shortcut_manager.(h|cc) to the AppShortcutManager, so web app
// shortcuts can be managed in an extensions agnostic way.
// Manages OS shortcuts for web applications.
class AppShortcutManager {
 public:
  explicit AppShortcutManager(Profile* profile);
  virtual ~AppShortcutManager();

  void SetSubsystems(AppRegistrar* registrar);

  void AddObserver(AppShortcutObserver* observer);
  void RemoveObserver(AppShortcutObserver* observer);

  // Tells the AppShortcutManager that no shortcuts should actually be written
  // to the disk.
  void SuppressShortcutsForTesting();

  virtual bool CanCreateShortcuts() const;

  // virtual for testing.
  virtual void CreateShortcuts(const AppId& app_id,
                               bool add_to_desktop,
                               CreateShortcutsCallback callback);

  // The result of a call to GetShortcutInfo.
  using GetShortcutInfoCallback =
      base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;
  // Asynchronously gets the information required to create a shortcut for
  // |app_id|.
  virtual void GetShortcutInfoForApp(const AppId& app_id,
                                     GetShortcutInfoCallback callback) = 0;

 protected:
  void OnShortcutsCreated(const AppId& app_id,
                          CreateShortcutsCallback callback,
                          bool success);

  AppRegistrar* registrar() { return registrar_; }
  Profile* profile() { return profile_; }

 private:
  void OnShortcutInfoRetrievedCreateShortcuts(
      bool add_to_desktop,
      CreateShortcutsCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  bool suppress_shortcuts_for_testing_ = false;

  AppRegistrar* registrar_ = nullptr;
  Profile* const profile_;

  base::ObserverList<AppShortcutObserver, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<AppShortcutManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppShortcutManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
