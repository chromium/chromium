// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_H_

#include <memory>

#include "ash/public/cpp/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/prefs/pref_change_registrar.h"

class AccountId;

// Handles chrome-side wallpaper control alongside the ash-side controller.
class WallpaperControllerClient : public ash::WallpaperControllerClient {
 public:
  WallpaperControllerClient();
  virtual ~WallpaperControllerClient();

  // Initializes and connects to ash.
  void Init();

  // Tests can provide a mock interface for the ash controller.
  void InitForTesting(ash::WallpaperController* controller);

  // Sets the initial wallpaper. Should be called after the session manager has
  // been initialized.
  void SetInitialWallpaper();

  static WallpaperControllerClient* Get();

  // Returns files identifier for the |account_id|.
  std::string GetFilesId(const AccountId& account_id) const;

  // Wrappers around the ash::WallpaperController interface.
  void SetCustomWallpaper(const AccountId& account_id,
                          const std::string& wallpaper_files_id,
                          const std::string& file_name,
                          ash::WallpaperLayout layout,
                          const gfx::ImageSkia& image,
                          bool preview_mode);
  void SetOnlineWallpaperIfExists(
      const AccountId& account_id,
      const std::string& url,
      ash::WallpaperLayout layout,
      bool preview_mode,
      ash::WallpaperController::SetOnlineWallpaperIfExistsCallback callback);
  void SetOnlineWallpaperFromData(
      const AccountId& account_id,
      const std::string& image_data,
      const std::string& url,
      ash::WallpaperLayout layout,
      bool preview_mode,
      ash::WallpaperController::SetOnlineWallpaperFromDataCallback callback);
  void SetDefaultWallpaper(const AccountId& account_id, bool show_wallpaper);
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_small_path,
      const base::FilePath& customized_default_large_path);
  void SetPolicyWallpaper(const AccountId& account_id,
                          std::unique_ptr<std::string> data);
  bool SetThirdPartyWallpaper(const AccountId& account_id,
                              const std::string& wallpaper_files_id,
                              const std::string& file_name,
                              ash::WallpaperLayout layout,
                              const gfx::ImageSkia& image);
  void ConfirmPreviewWallpaper();
  void CancelPreviewWallpaper();
  void UpdateCustomWallpaperLayout(const AccountId& account_id,
                                   ash::WallpaperLayout layout);
  void ShowUserWallpaper(const AccountId& account_id);
  void ShowSigninWallpaper();
  void ShowAlwaysOnTopWallpaper(const base::FilePath& image_path);
  void RemoveAlwaysOnTopWallpaper();
  void RemoveUserWallpaper(const AccountId& account_id);
  void RemovePolicyWallpaper(const AccountId& account_id);
  void GetOfflineWallpaperList(
      ash::WallpaperController::GetOfflineWallpaperListCallback callback);
  void SetAnimationDuration(const base::TimeDelta& animation_duration);
  void OpenWallpaperPickerIfAllowed();
  void MinimizeInactiveWindows(const std::string& user_id_hash);
  void RestoreMinimizedWindows(const std::string& user_id_hash);
  void AddObserver(ash::WallpaperControllerObserver* observer);
  void RemoveObserver(ash::WallpaperControllerObserver* observer);
  gfx::ImageSkia GetWallpaperImage();
  const std::vector<SkColor>& GetWallpaperColors();
  bool IsWallpaperBlurred();
  bool IsActiveUserWallpaperControlledByPolicy();
  ash::WallpaperInfo GetActiveUserWallpaperInfo();
  bool ShouldShowWallpaperSetting();

 private:
  // Initialize the controller for this client and some wallpaper directories.
  void InitController();

  // Shows the wallpaper of the first user in |UserManager::GetUsers|, or a
  // default signin wallpaper if there's no user. This ensures the wallpaper is
  // shown right after boot, regardless of when the login screen is available.
  void ShowWallpaperOnLoginScreen();

  // ash::WallpaperControllerClient:
  void OpenWallpaperPicker() override;

  void DeviceWallpaperImageFilePathChanged();

  // Returns true if user names should be shown on the login screen.
  bool ShouldShowUserNamesOnLogin() const;

  base::FilePath GetDeviceWallpaperImageFilePath();

  // WallpaperController interface in ash.
  ash::WallpaperController* wallpaper_controller_;

  PrefService* local_state_;

  // The registrar used to watch DeviceWallpaperImageFilePath pref changes.
  PrefChangeRegistrar pref_registrar_;

  // Observes if user names should be shown on the login screen, which
  // determines whether a user wallpaper or a default wallpaper should be shown.
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      show_user_names_on_signin_subscription_;

  base::WeakPtrFactory<WallpaperControllerClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_H_
