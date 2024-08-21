// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"

#include <optional>
#include <string>

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_drivefs_delegate.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "test_wallpaper_controller.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

TestWallpaperController::TestWallpaperController() : id_cache_(0) {
  ClearCounts();
}

TestWallpaperController::~TestWallpaperController() = default;

void TestWallpaperController::ShowWallpaperImage(const gfx::ImageSkia& image) {
  current_wallpaper = image;
  for (auto& observer : observers_)
    observer.OnWallpaperChanged();
}

void TestWallpaperController::ClearCounts() {
  set_online_wallpaper_count_ = 0;
  set_google_photos_wallpaper_count_ = 0;
  show_override_wallpaper_count_[/*always_on_top=*/false] = 0;
  show_override_wallpaper_count_[/*always_on_top=*/true] = 0;
  remove_override_wallpaper_count_ = 0;
  remove_user_wallpaper_count_ = 0;
  wallpaper_info_ = std::nullopt;
  update_current_wallpaper_layout_count_ = 0;
  update_current_wallpaper_layout_layout_ = std::nullopt;
  update_daily_refresh_wallpaper_count_ = 0;
  one_shot_wallpaper_count_ = 0;
  sea_pen_wallpaper_count_ = 0;
}

void TestWallpaperController::SetClient(
    ash::WallpaperControllerClient* client) {
  was_client_set_ = true;
}

void TestWallpaperController::SetDriveFsDelegate(
    std::unique_ptr<ash::WallpaperDriveFsDelegate> drivefs_delegate) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestWallpaperController::Init(
    const base::FilePath& user_data,
    const base::FilePath& wallpapers,
    const base::FilePath& custom_wallpapers,
    const base::FilePath& device_policy_wallpaper) {
  NOTIMPLEMENTED();
}

bool TestWallpaperController::CanSetUserWallpaper(
    const AccountId& account_id) const {
  return can_set_user_wallpaper_;
}

void TestWallpaperController::SetCustomWallpaper(
    const AccountId& account_id,
    const base::FilePath& file_path,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SetWallpaperCallback callback) {
  ++set_custom_wallpaper_count_;
  std::move(callback).Run(true);
}

void TestWallpaperController::SetDecodedCustomWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SetWallpaperCallback callback,
    const std::string& file_path,
    const gfx::ImageSkia& image) {
  ++set_custom_wallpaper_count_;
}

void TestWallpaperController::SetOnlineWallpaper(
    const ash::OnlineWallpaperParams& params,
    SetWallpaperCallback callback) {
  ++set_online_wallpaper_count_;
  CHECK(!params.variants.empty());
  wallpaper_info_ = ash::WallpaperInfo(params, params.variants.front());
  std::move(callback).Run(/*success=*/true);
}

void TestWallpaperController::ShowOobeWallpaper() {
  ++set_oobe_wallpaper_count_;
}

void TestWallpaperController::SetGooglePhotosWallpaper(
    const ash::GooglePhotosWallpaperParams& params,
    SetWallpaperCallback callback) {
  ++set_google_photos_wallpaper_count_;
  wallpaper_info_ = ash::WallpaperInfo(params);
  std::move(callback).Run(/*success=*/true);
}

void TestWallpaperController::SetGooglePhotosDailyRefreshAlbumId(
    const AccountId& account_id,
    const std::string& album_id) {
  if (!wallpaper_info_)
    wallpaper_info_ = ash::WallpaperInfo();
  wallpaper_info_->type = ash::WallpaperType::kDailyGooglePhotos;
  wallpaper_info_->collection_id = album_id;
}

std::string TestWallpaperController::GetGooglePhotosDailyRefreshAlbumId(
    const AccountId& account_id) const {
  if (!wallpaper_info_.has_value() ||
      wallpaper_info_->type != ash::WallpaperType::kDailyGooglePhotos) {
    return std::string();
  }
  return wallpaper_info_->collection_id;
}

bool TestWallpaperController::SetDailyGooglePhotosWallpaperIdCache(
    const AccountId& account_id,
    const DailyGooglePhotosIdCache& ids) {
  id_cache_.ShrinkToSize(0);
  base::ranges::for_each(base::Reversed(ids),
                         [&](uint id) { id_cache_.Put(std::move(id)); });
  return true;
}

bool TestWallpaperController::GetDailyGooglePhotosWallpaperIdCache(
    const AccountId& account_id,
    DailyGooglePhotosIdCache& ids_out) const {
  base::ranges::for_each(base::Reversed(id_cache_),
                         [&](uint id) { ids_out.Put(std::move(id)); });
  return true;
}

void TestWallpaperController::SetTimeOfDayWallpaper(
    const AccountId& account_id,
    SetWallpaperCallback callback) {
  ++set_default_time_of_day_wallpaper_count_;
  std::move(callback).Run(/*success=*/true);
}

void TestWallpaperController::SetDefaultWallpaper(
    const AccountId& account_id,
    bool show_wallpaper,
    SetWallpaperCallback callback) {
  ++set_default_wallpaper_count_;
  std::move(callback).Run(/*success=*/true);
}

base::FilePath TestWallpaperController::GetDefaultWallpaperPath(
    user_manager::UserType) {
  return base::FilePath();
}

void TestWallpaperController::SetCustomizedDefaultWallpaperPaths(
    const base::FilePath& customized_default_small_path,
    const base::FilePath& customized_default_large_path) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetPolicyWallpaper(
    const AccountId& account_id,
    user_manager::UserType user_type,
    const std::string& data) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetDevicePolicyWallpaperPath(
    const base::FilePath& device_policy_wallpaper_path) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetCurrentUser(const AccountId& account_id) {
  current_account_id = account_id;
}

bool TestWallpaperController::SetThirdPartyWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image) {
  if (current_account_id != account_id) {
    return false;
  }
  ShowWallpaperImage(image);
  ++third_party_wallpaper_count_;
  return true;
}

void TestWallpaperController::SetSeaPenWallpaper(
    const AccountId& account_id,
    const uint32_t image_id,
    bool preview_mode,
    SetWallpaperCallback callback) {
  ++sea_pen_wallpaper_count_;

  wallpaper_info_ = ash::WallpaperInfo();
  wallpaper_info_->type = ash::WallpaperType::kSeaPen;
  wallpaper_info_->location = base::NumberToString(image_id);
  std::move(callback).Run(/*success=*/true);
}

void TestWallpaperController::ConfirmPreviewWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::CancelPreviewWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::UpdateCurrentWallpaperLayout(
    const AccountId& account_id,
    ash::WallpaperLayout layout) {
  ++update_current_wallpaper_layout_count_;
  update_current_wallpaper_layout_layout_ = layout;
}

void TestWallpaperController::ShowUserWallpaper(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowUserWallpaper(
    const AccountId& account_id,
    user_manager::UserType user_type) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowSigninWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowOneShotWallpaper(
    const gfx::ImageSkia& image) {
  ++one_shot_wallpaper_count_;
  ShowWallpaperImage(image);
}

void TestWallpaperController::ShowOverrideWallpaper(
    const base::FilePath& image_path,
    bool always_on_top) {
  ++show_override_wallpaper_count_[always_on_top];
}

void TestWallpaperController::RemoveOverrideWallpaper() {
  ++remove_override_wallpaper_count_;
}

void TestWallpaperController::RemoveUserWallpaper(
    const AccountId& account_id,
    base::OnceClosure on_removed) {
  ++remove_user_wallpaper_count_;
}

void TestWallpaperController::RemovePolicyWallpaper(
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetAnimationDuration(
    base::TimeDelta animation_duration) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::OpenWallpaperPickerIfAllowed() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::AddObserver(
    ash::WallpaperControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void TestWallpaperController::RemoveObserver(
    ash::WallpaperControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::ImageSkia TestWallpaperController::GetWallpaperImage() {
  return current_wallpaper;
}

void TestWallpaperController::LoadPreviewImage(
    LoadPreviewImageCallback callback) {
  current_wallpaper.MakeThreadSafe();
  std::move(callback).Run(gfx::Image(current_wallpaper).As1xPNGBytes());
}

bool TestWallpaperController::IsWallpaperBlurredForLockState() const {
  NOTIMPLEMENTED();
  return false;
}

bool TestWallpaperController::IsActiveUserWallpaperControlledByPolicy() {
  NOTIMPLEMENTED();
  return false;
}

bool TestWallpaperController::IsWallpaperControlledByPolicy(
    const AccountId& account_id) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

std::optional<ash::WallpaperInfo>
TestWallpaperController::GetActiveUserWallpaperInfo() const {
  return wallpaper_info_;
}

std::optional<ash::WallpaperInfo>
TestWallpaperController::GetWallpaperInfoForAccountId(
    const AccountId& account_id) const {
  return wallpaper_info_;
}

void TestWallpaperController::SetDailyRefreshCollectionId(
    const AccountId& account_id,
    const std::string& collection_id) {
  if (!wallpaper_info_)
    wallpaper_info_ = ash::WallpaperInfo();
  wallpaper_info_->type = ash::WallpaperType::kDaily;
  wallpaper_info_->collection_id = collection_id;
}

std::string TestWallpaperController::GetDailyRefreshCollectionId(
    const AccountId& account_id) const {
  if (!wallpaper_info_.has_value() ||
      wallpaper_info_->type != ash::WallpaperType::kDaily) {
    return std::string();
  }
  return wallpaper_info_->collection_id;
}

void TestWallpaperController::UpdateDailyRefreshWallpaper(
    RefreshWallpaperCallback callback) {
  update_daily_refresh_wallpaper_count_++;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
}

void TestWallpaperController::SyncLocalAndRemotePrefs(
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

const AccountId& TestWallpaperController::CurrentAccountId() const {
  return current_account_id;
}
