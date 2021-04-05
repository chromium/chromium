// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/components/web_application_info.h"

class Profile;

namespace web_app {

class AppIconManager;
struct ShortcutInfo;

using ShortcutLocationCallback =
    base::OnceCallback<void(ShortcutLocations shortcut_locations)>;

// This class manages creation/update/deletion of OS shortcuts for web
// applications.
//
// TODO(crbug.com/860581): Migrate functions from
// web_app_extension_shortcut.(h|cc) and
// platform_apps/shortcut_manager.(h|cc) to AppShortcutManager and
// its subclasses.
class AppShortcutManager {
 public:
  explicit AppShortcutManager(Profile* profile);
  AppShortcutManager(const AppShortcutManager&) = delete;
  AppShortcutManager& operator=(const AppShortcutManager&) = delete;
  virtual ~AppShortcutManager();

  void SetSubsystems(AppIconManager* icon_manager, AppRegistrar* registrar);

  void Start();
  void Shutdown();

  // Tells the AppShortcutManager that no shortcuts should actually be written
  // to the disk.
  void SuppressShortcutsForTesting();

  bool CanCreateShortcuts() const;
  void CreateShortcuts(const AppId& app_id,
                       bool add_to_desktop,
                       CreateShortcutsCallback callback);
  // Fetch already-updated shortcut data and deploy to OS integration.
  void UpdateShortcuts(const AppId& app_id, base::StringPiece old_name);
  void DeleteShortcuts(const AppId& app_id,
                       const base::FilePath& shortcuts_data_dir,
                       std::unique_ptr<ShortcutInfo> shortcut_info,
                       DeleteShortcutsCallback callback);

  // Posts a task on the IO thread to gather existing shortcut locations
  // according to |shortcut_info|. The result will be passed into |callback|.
  // virtual for testing.
  virtual void GetAppExistingShortCutLocation(
      ShortcutLocationCallback callback,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  // TODO(crbug.com/1098471): Move this into web_app_shortcuts_menu_win.cc when
  // a callback is integrated into the Shortcuts Menu registration flow.
  using RegisterShortcutsMenuCallback = base::OnceCallback<void(bool success)>;
  // Registers a shortcuts menu for a web app after reading its shortcuts menu
  // icons from disk.
  //
  // TODO(crbug.com/1098471): Consider unifying this method and
  // RegisterShortcutsMenuWithOs() below.
  void ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      const AppId& app_id,
      RegisterShortcutsMenuCallback callback);

  // Registers a shortcuts menu for the web app's icon with the OS.
  //
  // TODO(crbug.com/1098471): Add a callback as part of the Shortcuts Menu
  // registration flow.
  void RegisterShortcutsMenuWithOs(
      const AppId& app_id,
      const std::vector<WebApplicationShortcutsMenuItemInfo>&
          shortcuts_menu_item_infos,
      const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps);

  void UnregisterShortcutsMenuWithOs(const AppId& app_id);

  // Builds initial ShortcutInfo without |ShortcutInfo::favicon| being read.
  virtual std::unique_ptr<ShortcutInfo> BuildShortcutInfo(
      const AppId& app_id) = 0;

  // The result of a call to GetShortcutInfo.
  using GetShortcutInfoCallback =
      base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;
  // Asynchronously gets the information required to create a shortcut for
  // |app_id| including all the icon bitmaps. Returns nullptr if app_id is
  // uninstalled or becomes uninstalled during the asynchronous read of icons.
  virtual void GetShortcutInfoForApp(const AppId& app_id,
                                     GetShortcutInfoCallback callback) = 0;

  using ShortcutCallback = base::OnceCallback<void(const ShortcutInfo*)>;
  static void SetShortcutUpdateCallbackForTesting(ShortcutCallback callback);

 protected:
  void OnShortcutsCreated(const AppId& app_id,
                          CreateShortcutsCallback callback,
                          bool success);
  void OnShortcutsDeleted(const AppId& app_id,
                          DeleteShortcutsCallback callback,
                          bool success);

  AppRegistrar* registrar() { return registrar_; }
  Profile* profile() { return profile_; }
  bool suppress_shortcuts_for_testing() const {
    return suppress_shortcuts_for_testing_;
  }

 private:
  void OnShortcutInfoRetrievedCreateShortcuts(
      bool add_to_desktop,
      CreateShortcutsCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutInfoRetrievedUpdateShortcuts(
      std::u16string old_name,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutsMenuIconsReadRegisterShortcutsMenu(
      const AppId& app_id,
      RegisterShortcutsMenuCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  bool suppress_shortcuts_for_testing_ = false;

  AppRegistrar* registrar_ = nullptr;
  AppIconManager* icon_manager_ = nullptr;
  Profile* const profile_;

  base::WeakPtrFactory<AppShortcutManager> weak_ptr_factory_{this};

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
