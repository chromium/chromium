// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace web_app {

class WebAppFileHandlerManager;
class WebAppProtocolHandlerManager;
class WebApp;
class WebAppIconManager;
struct ShortcutInfo;

using ShortcutLocationCallback =
    base::OnceCallback<void(ShortcutLocations shortcut_locations)>;

// This class manages creation/update/deletion of OS shortcuts for web
// applications.
//
// TODO(crbug.com/860581): Migrate functions from
// web_app_extension_shortcut.(h|cc) and
// platform_apps/shortcut_manager.(h|cc) to WebAppShortcutManager.
class WebAppShortcutManager {
 public:
  WebAppShortcutManager(Profile* profile,
                        WebAppIconManager* icon_manager,
                        WebAppFileHandlerManager* file_handler_manager,
                        WebAppProtocolHandlerManager* protocol_handler_manager);
  WebAppShortcutManager(const WebAppShortcutManager&) = delete;
  WebAppShortcutManager& operator=(const WebAppShortcutManager&) = delete;
  virtual ~WebAppShortcutManager();

  void SetSubsystems(WebAppIconManager* icon_manager,
                     WebAppRegistrar* registrar);

  void Start();
  void Shutdown();

  // Tells the WebAppShortcutManager that no shortcuts should actually be
  // written to the disk.
  void SuppressShortcutsForTesting();

  bool CanCreateShortcuts() const;
  void CreateShortcuts(const AppId& app_id,
                       bool add_to_desktop,
                       CreateShortcutsCallback callback);
  // Fetch already-updated shortcut data and deploy to OS integration.
  void UpdateShortcuts(const AppId& app_id,
                       base::StringPiece old_name,
                       base::OnceClosure update_finished_callback);
  void DeleteShortcuts(const AppId& app_id,
                       const base::FilePath& shortcuts_data_dir,
                       std::unique_ptr<ShortcutInfo> shortcut_info,
                       ResultCallback callback);

  // Posts a task on the IO thread to gather existing shortcut locations
  // according to |shortcut_info|. The result will be passed into |callback|.
  // virtual for testing.
  virtual void GetAppExistingShortCutLocation(
      ShortcutLocationCallback callback,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  // TODO(crbug.com/1098471): Move this into web_app_shortcuts_menu_win.cc when
  // a callback is integrated into the Shortcuts Menu registration flow.
  using RegisterShortcutsMenuCallback = base::OnceCallback<void(Result result)>;
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
      const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
      const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps);

  void UnregisterShortcutsMenuWithOs(const AppId& app_id);

  // Builds initial ShortcutInfo without |ShortcutInfo::favicon| being read.
  // virtual for testing.
  //
  // TODO(crbug.com/1225132): Get rid of |BuildShortcutInfo| method: inline it
  // or make it private.
  virtual std::unique_ptr<ShortcutInfo> BuildShortcutInfo(const AppId& app_id);

  // The result of a call to GetShortcutInfo.
  using GetShortcutInfoCallback =
      base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;
  // Asynchronously gets the information required to create a shortcut for
  // |app_id| including all the icon bitmaps. Returns nullptr if app_id is
  // uninstalled or becomes uninstalled during the asynchronous read of icons.
  // virtual for testing.
  virtual void GetShortcutInfoForApp(const AppId& app_id,
                                     GetShortcutInfoCallback callback);

  using ShortcutCallback = base::OnceCallback<void(const ShortcutInfo*)>;
  static void SetShortcutUpdateCallbackForTesting(ShortcutCallback callback);

 private:
  void OnIconsRead(const AppId& app_id,
                   GetShortcutInfoCallback callback,
                   std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  void OnShortcutsCreated(const AppId& app_id,
                          CreateShortcutsCallback callback,
                          bool success);
  void OnShortcutsDeleted(const AppId& app_id,
                          ResultCallback callback,
                          bool success);

  void OnShortcutInfoRetrievedCreateShortcuts(
      bool add_to_desktop,
      CreateShortcutsCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutInfoRetrievedUpdateShortcuts(
      std::u16string old_name,
      base::OnceClosure update_finished_callback,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutsMenuIconsReadRegisterShortcutsMenu(
      const AppId& app_id,
      RegisterShortcutsMenuCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  std::unique_ptr<ShortcutInfo> BuildShortcutInfoForWebApp(const WebApp* app);

  bool suppress_shortcuts_for_testing_ = false;

  const raw_ptr<Profile> profile_;

  raw_ptr<WebAppRegistrar> registrar_ = nullptr;
  raw_ptr<WebAppIconManager> icon_manager_ = nullptr;
  raw_ptr<WebAppFileHandlerManager> file_handler_manager_ = nullptr;
  raw_ptr<WebAppProtocolHandlerManager> protocol_handler_manager_ = nullptr;

  base::WeakPtrFactory<WebAppShortcutManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MANAGER_H_
