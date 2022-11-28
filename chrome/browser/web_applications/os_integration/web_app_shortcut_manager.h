// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MANAGER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
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
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

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

  // Tells the WebAppShortcutManager that no shortcuts should actually be
  // written to the disk.
  void SuppressShortcutsForTesting();

  bool CanCreateShortcuts() const;
  void CreateShortcuts(const AppId& app_id,
                       bool add_to_desktop,
                       ShortcutCreationReason reason,
                       CreateShortcutsCallback callback);
  // Fetch already-updated shortcut data and deploy to OS integration.
  void UpdateShortcuts(const AppId& app_id,
                       base::StringPiece old_name,
                       ResultCallback update_finished_callback);
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

  // Registers a shortcuts menu for a web app after reading its shortcuts menu
  // icons from disk.
  //
  // TODO(crbug.com/1098471): Consider unifying this method and
  // RegisterShortcutsMenuWithOs() below.
  void ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      const AppId& app_id,
      ResultCallback callback);

  // Registers a shortcuts menu for the web app's icon with the OS.
  void RegisterShortcutsMenuWithOs(
      const AppId& app_id,
      const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
      const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
      ResultCallback callback);

  void UnregisterShortcutsMenuWithOs(const AppId& app_id,
                                     ResultCallback callback);

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

  // Sets a callback to be called when this class determines that all shortcuts
  // for a particular profile need to be rebuild, for example because the app
  // shortcut version has changed since the last time these were created.
  // This is used by the legacy extensions based app code in
  // chrome/browser/web_applications/extensions to ensure those app shortcuts
  // also get updated. Calling out to that code directly would violate
  // dependency layering.
  using UpdateShortcutsForAllAppsCallback =
      base::RepeatingCallback<void(Profile*, base::OnceClosure)>;
  static void SetUpdateShortcutsForAllAppsCallback(
      UpdateShortcutsForAllAppsCallback callback);

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
      ShortcutCreationReason reason,
      CreateShortcutsCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutInfoRetrievedUpdateShortcuts(
      std::u16string old_name,
      ResultCallback update_finished_callback,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutsMenuIconsReadRegisterShortcutsMenu(
      const AppId& app_id,
      ResultCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  std::unique_ptr<ShortcutInfo> BuildShortcutInfoForWebApp(const WebApp* app);

  // Schedules a call to UpdateShortcutsForAllAppsNow() if kAppShortcutsVersion
  // in prefs is less than kCurrentAppShortcutsVersion.
  void UpdateShortcutsForAllAppsIfNeeded();
  void UpdateShortcutsForAllAppsNow();
  void SetCurrentAppShortcutsVersion();

  bool suppress_shortcuts_for_testing_ = false;

  const raw_ptr<Profile> profile_;

  raw_ptr<WebAppRegistrar> registrar_ = nullptr;
  raw_ptr<WebAppIconManager> icon_manager_ = nullptr;
  raw_ptr<WebAppFileHandlerManager, DanglingUntriaged> file_handler_manager_ =
      nullptr;
  raw_ptr<WebAppProtocolHandlerManager, DanglingUntriaged>
      protocol_handler_manager_ = nullptr;

  base::WeakPtrFactory<WebAppShortcutManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MANAGER_H_
