// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_H_

#include <map>
#include <optional>
#include <string>

#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_drivefs_delegate.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "ui/gfx/image/image_skia.h"

// Simulates WallpaperController in ash.
class TestWallpaperController : public ash::WallpaperController {
 public:
  TestWallpaperController();

  TestWallpaperController(const TestWallpaperController&) = delete;
  TestWallpaperController& operator=(const TestWallpaperController&) = delete;

  ~TestWallpaperController() override;

  // Simulates showing the wallpaper on screen by updating |current_wallpaper|
  // and notifying the observers.
  void ShowWallpaperImage(const gfx::ImageSkia& image);

  void ClearCounts();
  bool was_client_set() const { return was_client_set_; }
  void set_can_set_user_wallpaper(bool can_set_user_wallpaper) {
    can_set_user_wallpaper_ = can_set_user_wallpaper;
  }
  int remove_user_wallpaper_count() const {
    return remove_user_wallpaper_count_;
  }
  int set_default_time_of_day_wallpaper_count() const {
    return set_default_time_of_day_wallpaper_count_;
  }
  int set_default_wallpaper_count() const {
    return set_default_wallpaper_count_;
  }
  int set_custom_wallpaper_count() const { return set_custom_wallpaper_count_; }
  int set_online_wallpaper_count() const { return set_online_wallpaper_count_; }
  int set_google_photos_wallpaper_count() const {
    return set_google_photos_wallpaper_count_;
  }
  int get_third_party_wallpaper_count() const {
    return third_party_wallpaper_count_;
  }
  int get_update_daily_refresh_wallpaper_count() const {
    return update_daily_refresh_wallpaper_count_;
  }
  int get_one_shot_wallpaper_count() const { return one_shot_wallpaper_count_; }
  int get_sea_pen_wallpaper_count() const { return sea_pen_wallpaper_count_; }
  int show_override_wallpaper_count() const {
    return show_override_wallpaper_count(/*always_on_top=*/false) +
           show_override_wallpaper_count(/*always_on_top=*/true);
  }
  int show_override_wallpaper_count(bool always_on_top) const {
    return show_override_wallpaper_count_.at(always_on_top);
  }
  int remove_override_wallpaper_count() const {
    return remove_override_wallpaper_count_;
  }
  const std::string& collection_id() const {
    return wallpaper_info_.has_value() ? wallpaper_info_->collection_id
                                       : base::EmptyString();
  }
  const std::optional<ash::WallpaperInfo>& wallpaper_info() const {
    return wallpaper_info_;
  }
  int update_current_wallpaper_layout_count() const {
    return update_current_wallpaper_layout_count_;
  }
  const std::optional<ash::WallpaperLayout>&
  update_current_wallpaper_layout_layout() const {
    return update_current_wallpaper_layout_layout_;
  }
  void add_dedup_key_to_wallpaper_info(const std::string& dedup_key) {
    if (wallpaper_info_.has_value())
      wallpaper_info_->dedup_key = dedup_key;
  }

  // ash::WallpaperController:
  void SetClient(ash::WallpaperControllerClient* client) override;
  void SetDriveFsDelegate(
      std::unique_ptr<ash::WallpaperDriveFsDelegate> drivefs_delegate) override;
  void Init(const base::FilePath& user_data,
            const base::FilePath& wallpapers,
            const base::FilePath& custom_wallpapers,
            const base::FilePath& device_policy_wallpaper) override;
  bool CanSetUserWallpaper(const AccountId& account_id) const override;
  void SetCustomWallpaper(const AccountId& account_id,
                          const base::FilePath& file_path,
                          ash::WallpaperLayout layout,
                          bool preview_mode,
                          SetWallpaperCallback callback) override;
  void SetDecodedCustomWallpaper(const AccountId& account_id,
                                 const std::string& file_name,
                                 ash::WallpaperLayout layout,
                                 bool preview_mode,
                                 SetWallpaperCallback callback,
                                 const std::string& file_path,
                                 const gfx::ImageSkia& image) override;
  void SetOnlineWallpaper(const ash::OnlineWallpaperParams& params,
                          SetWallpaperCallback callback) override;
  void ShowOobeWallpaper() override;
  void SetGooglePhotosWallpaper(const ash::GooglePhotosWallpaperParams& params,
                                SetWallpaperCallback callback) override;
  void SetGooglePhotosDailyRefreshAlbumId(const AccountId& account_id,
                                          const std::string& album_id) override;
  std::string GetGooglePhotosDailyRefreshAlbumId(
      const AccountId& account_id) const override;
  bool SetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      const DailyGooglePhotosIdCache& ids) override;
  bool GetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      DailyGooglePhotosIdCache& ids_out) const override;
  void SetCurrentUser(const AccountId& account_id);
  void SetTimeOfDayWallpaper(const AccountId& account_id,
                             SetWallpaperCallback callback) override;
  void SetDefaultWallpaper(const AccountId& account_id,
                           bool show_wallpaper,
                           SetWallpaperCallback callback) override;
  base::FilePath GetDefaultWallpaperPath(
      user_manager::UserType user_type) override;
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_small_path,
      const base::FilePath& customized_default_large_path) override;
  void SetPolicyWallpaper(const AccountId& account_id,
                          user_manager::UserType user_type,
                          const std::string& data) override;
  void SetDevicePolicyWallpaperPath(
      const base::FilePath& device_policy_wallpaper_path) override;
  // Call `SetCurrentUser` with the account_id before calling
  // SetThirdPartyWallpaper, or else it will fail.
  bool SetThirdPartyWallpaper(const AccountId& account_id,
                              const std::string& file_name,
                              ash::WallpaperLayout layout,
                              const gfx::ImageSkia& image) override;
  void SetSeaPenWallpaper(const AccountId& account_id,
                          uint32_t id,
                          bool preview_mode,
                          SetWallpaperCallback callback) override;
  void ConfirmPreviewWallpaper() override;
  void CancelPreviewWallpaper() override;
  void UpdateCurrentWallpaperLayout(const AccountId& account_id,
                                    ash::WallpaperLayout layout) override;
  void ShowUserWallpaper(const AccountId& account_id) override;
  void ShowUserWallpaper(const AccountId& account_id,
                         user_manager::UserType user_type) override;
  void ShowSigninWallpaper() override;
  void ShowOneShotWallpaper(const gfx::ImageSkia& image) override;
  void ShowOverrideWallpaper(const base::FilePath& image_path,
                             bool always_on_top) override;
  void RemoveOverrideWallpaper() override;
  void RemoveUserWallpaper(const AccountId& account_id,
                           base::OnceClosure on_removed) override;
  void RemovePolicyWallpaper(const AccountId& account_id) override;
  void SetAnimationDuration(base::TimeDelta animation_duration) override;
  void OpenWallpaperPickerIfAllowed() override;
  void MinimizeInactiveWindows(const std::string& user_id_hash) override;
  void RestoreMinimizedWindows(const std::string& user_id_hash) override;
  void AddObserver(ash::WallpaperControllerObserver* observer) override;
  void RemoveObserver(ash::WallpaperControllerObserver* observer) override;
  gfx::ImageSkia GetWallpaperImage() override;
  void LoadPreviewImage(LoadPreviewImageCallback callback) override;
  bool IsWallpaperBlurredForLockState() const override;
  bool IsActiveUserWallpaperControlledByPolicy() override;
  bool IsWallpaperControlledByPolicy(
      const AccountId& account_id) const override;
  std::optional<ash::WallpaperInfo> GetActiveUserWallpaperInfo() const override;
  std::optional<ash::WallpaperInfo> GetWallpaperInfoForAccountId(
      const AccountId& account_id) const override;
  void SetDailyRefreshCollectionId(const AccountId& account_id,
                                   const std::string& collection_id) override;
  std::string GetDailyRefreshCollectionId(
      const AccountId& account_id) const override;
  void UpdateDailyRefreshWallpaper(RefreshWallpaperCallback callback) override;
  void SyncLocalAndRemotePrefs(const AccountId& account_id) override;
  const AccountId& CurrentAccountId() const override;

 private:
  bool was_client_set_ = false;
  bool can_set_user_wallpaper_ = true;
  int remove_user_wallpaper_count_ = 0;
  int set_default_time_of_day_wallpaper_count_ = 0;
  int set_default_wallpaper_count_ = 0;
  int set_custom_wallpaper_count_ = 0;
  int set_online_wallpaper_count_ = 0;
  int set_oobe_wallpaper_count_ = 0;
  int set_google_photos_wallpaper_count_ = 0;
  std::map</*always_on_top=*/bool, int> show_override_wallpaper_count_;
  int remove_override_wallpaper_count_ = 0;
  int third_party_wallpaper_count_ = 0;
  int update_daily_refresh_wallpaper_count_ = 0;
  int one_shot_wallpaper_count_ = 0;
  int sea_pen_wallpaper_count_ = 0;
  std::optional<ash::WallpaperInfo> wallpaper_info_;
  int update_current_wallpaper_layout_count_ = 0;
  std::optional<ash::WallpaperLayout> update_current_wallpaper_layout_layout_;
  DailyGooglePhotosIdCache id_cache_;

  base::ObserverList<ash::WallpaperControllerObserver>::Unchecked observers_;

  AccountId current_account_id;
  gfx::ImageSkia current_wallpaper;
};

#endif  // CHROME_BROWSER_UI_ASH_WALLPAPER_TEST_WALLPAPER_CONTROLLER_H_
