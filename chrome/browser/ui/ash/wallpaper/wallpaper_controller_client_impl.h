// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_IMPL_H_

#include <memory>

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"

class AccountId;

namespace {
class WallpaperControllerClientImplTest;
}

namespace content {
class WebContents;
}  // namespace content

namespace wallpaper_handlers {
class WallpaperFetcherDelegate;
}  // namespace wallpaper_handlers

// Handles chrome-side wallpaper control alongside the ash-side controller.
class WallpaperControllerClientImpl
    : public ash::WallpaperControllerClient,
      public file_manager::VolumeManagerObserver,
      public session_manager::SessionManagerObserver,
      public user_manager::UserManager::Observer {
 public:
  explicit WallpaperControllerClientImpl(
      std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
          wallpaper_fetcher_delegate);

  WallpaperControllerClientImpl(const WallpaperControllerClientImpl&) = delete;
  WallpaperControllerClientImpl& operator=(
      const WallpaperControllerClientImpl&) = delete;

  ~WallpaperControllerClientImpl() override;

  // Initializes and connects to ash.
  void Init();

  // Tests can provide a mock interface for the ash controller.
  void InitForTesting(ash::WallpaperController* controller);

  void SetWallpaperFetcherDelegateForTesting(
      std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>);

  // Sets the initial wallpaper. Should be called after the session manager has
  // been initialized.
  void SetInitialWallpaper();

  static WallpaperControllerClientImpl* Get();

  // ash::WallpaperControllerClient:
  void OpenWallpaperPicker() override;
  void FetchDailyRefreshWallpaper(
      const std::string& collection_id,
      DailyWallpaperUrlFetchedCallback callback) override;
  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;
  void FetchGooglePhotosPhoto(const AccountId& account_id,
                              const std::string& id,
                              FetchGooglePhotosPhotoCallback callback) override;
  void FetchDailyGooglePhotosPhoto(
      const AccountId& account_id,
      const std::string& album_id,
      FetchGooglePhotosPhotoCallback callback) override;
  void FetchGooglePhotosAccessToken(
      const AccountId& account_id,
      FetchGooglePhotosAccessTokenCallback callback) override;
  void GetFilesId(const AccountId& account_id,
                  base::OnceCallback<void(const std::string&)>
                      files_id_callback) const override;
  bool IsWallpaperSyncEnabled(const AccountId& account_id) const override;

  void CancelPreviewWallpaper(Profile* profile);
  void ConfirmPreviewWallpaper(Profile* profile);
  void MakeOpaque(content::WebContents* web_contents);
  void MakeTransparent(content::WebContents* web_contents);

  // file_manager::VolumeManagerObserver:
  void OnVolumeMounted(ash::MountError error_code,
                       const file_manager::Volume& volume) override;

  // session_manager::SessionManagerObserver implementation.
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // user_manager::UserManager::Observer:
  void OnUserLoggedIn(const user_manager::User& user) override;

  // Wrappers around the ash::WallpaperController interface.
  void SetPolicyWallpaper(const AccountId& account_id,
                          std::unique_ptr<std::string> data);
  bool SetThirdPartyWallpaper(const AccountId& account_id,
                              const std::string& file_name,
                              ash::WallpaperLayout layout,
                              const gfx::ImageSkia& image);
  void ShowUserWallpaper(const AccountId& account_id);
  void RemoveUserWallpaper(const AccountId& account_id,
                           base::OnceClosure on_removed);
  void RemovePolicyWallpaper(const AccountId& account_id);
  // Record Ash.Wallpaper.Source metric when a new wallpaper is set,
  // either by built-in Wallpaper app or a third party extension/app.
  void RecordWallpaperSourceUMA(const ash::WallpaperType type);

 private:
  friend class WallpaperControllerClientImplTest;
  // Initialize the controller for this client and some wallpaper directories.
  void InitController();

  // Shows the wallpaper of the first user in |UserManager::GetUsers|, or a
  // default signin wallpaper if there's no user. This ensures the wallpaper is
  // shown right after boot, regardless of when the login screen is available.
  void ShowWallpaperOnLoginScreen();

  void DeviceWallpaperImageFilePathChanged();

  // Returns true if user names should be shown on the login screen.
  bool ShouldShowUserNamesOnLogin() const;

  base::FilePath GetDeviceWallpaperImageFilePath();

  void OnDailyImageInfoFetched(DailyWallpaperUrlFetchedCallback callback,
                               bool success,
                               const backdrop::Image& image,
                               const std::string& next_resume_token);
  void OnFetchImagesForCollection(
      FetchImagesForCollectionCallback callback,
      std::unique_ptr<wallpaper_handlers::BackdropImageInfoFetcher> fetcher,
      bool success,
      const std::string& collection_id,
      const std::vector<backdrop::Image>& images);

  void OnGooglePhotosPhotoFetched(
      FetchGooglePhotosPhotoCallback callback,
      ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr
          response);

  void OnGooglePhotosDailyAlbumFetched(
      const AccountId& account_id,
      FetchGooglePhotosPhotoCallback callback,
      ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr
          response);

  void ObserveVolumeManagerForAccountId(const AccountId& account_id);

  // WallpaperController interface in ash.
  raw_ptr<ash::WallpaperController> wallpaper_controller_;

  raw_ptr<PrefService> local_state_;

  // The registrar used to watch DeviceWallpaperImageFilePath pref changes.
  PrefChangeRegistrar pref_registrar_;

  // Subscription for a callback that monitors if user names should be shown on
  // the login screen, which determines whether a user wallpaper or a default
  // wallpaper should be shown.
  base::CallbackListSubscription show_user_names_on_signin_subscription_;

  std::map<std::string,
           std::unique_ptr<wallpaper_handlers::BackdropSurpriseMeImageFetcher>>
      surprise_me_image_fetchers_;

  std::map<AccountId,
           std::unique_ptr<wallpaper_handlers::GooglePhotosPhotosFetcher>>
      google_photos_photos_fetchers_;

  std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
      wallpaper_fetcher_delegate_;

  base::ScopedMultiSourceObservation<file_manager::VolumeManager,
                                     file_manager::VolumeManagerObserver>
      volume_manager_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};

  base::WeakPtrFactory<WallpaperControllerClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_WALLPAPER_WALLPAPER_CONTROLLER_CLIENT_IMPL_H_
