// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_wallpaper_controller.h"

constexpr uint32_t dummy_image_id = 1;

TestWallpaperController::TestWallpaperController() : binding_(this) {}

TestWallpaperController::~TestWallpaperController() = default;

void TestWallpaperController::ShowWallpaperImage(const gfx::ImageSkia& image) {
  current_wallpaper = image;
  test_observers_.ForAllPtrs([](ash::mojom::WallpaperObserver* observer) {
    observer->OnWallpaperChanged(dummy_image_id);
  });
}

void TestWallpaperController::ClearCounts() {
  remove_user_wallpaper_count_ = 0;
}

ash::mojom::WallpaperControllerPtr
TestWallpaperController::CreateInterfacePtr() {
  ash::mojom::WallpaperControllerPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestWallpaperController::Init(
    ash::mojom::WallpaperControllerClientPtr client,
    const base::FilePath& user_data_path,
    const base::FilePath& chromeos_wallpapers_path,
    const base::FilePath& chromeos_custom_wallpapers_path,
    const base::FilePath& device_policy_wallpaper_path,
    bool is_device_wallpaper_policy_enforced) {
  was_client_set_ = true;
}

void TestWallpaperController::SetCustomWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image,
    bool preview_mode) {
  ++set_custom_wallpaper_count_;
}

void TestWallpaperController::SetOnlineWallpaperIfExists(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& url,
    ash::WallpaperLayout layout,
    bool preview_mode,
    ash::mojom::WallpaperController::SetOnlineWallpaperIfExistsCallback
        callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetOnlineWallpaperFromData(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& image_data,
    const std::string& url,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SetOnlineWallpaperFromDataCallback callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetDefaultWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    bool show_wallpaper) {
  ++set_default_wallpaper_count_;
}

void TestWallpaperController::SetCustomizedDefaultWallpaperPaths(
    const base::FilePath& customized_default_small_path,
    const base::FilePath& customized_default_large_path) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetPolicyWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& data) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetDeviceWallpaperPolicyEnforced(bool enforced) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetThirdPartyWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image,
    ash::mojom::WallpaperController::SetThirdPartyWallpaperCallback callback) {
  std::move(callback).Run(true /*allowed=*/, dummy_image_id);
  ShowWallpaperImage(image);
}

void TestWallpaperController::ConfirmPreviewWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::CancelPreviewWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::UpdateCustomWallpaperLayout(
    ash::mojom::WallpaperUserInfoPtr user_info,
    ash::WallpaperLayout layout) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowUserWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowSigninWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowOneShotWallpaper(
    const gfx::ImageSkia& image) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::RemoveUserWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id) {
  ++remove_user_wallpaper_count_;
}

void TestWallpaperController::RemovePolicyWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::GetOfflineWallpaperList(
    ash::mojom::WallpaperController::GetOfflineWallpaperListCallback callback) {
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
    ash::mojom::WallpaperObserverAssociatedPtrInfo observer) {
  ash::mojom::WallpaperObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  test_observers_.AddPtr(std::move(observer_ptr));
}

void TestWallpaperController::GetWallpaperImage(
    ash::mojom::WallpaperController::GetWallpaperImageCallback callback) {
  std::move(callback).Run(current_wallpaper);
}

void TestWallpaperController::GetWallpaperColors(
    ash::mojom::WallpaperController::GetWallpaperColorsCallback callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::IsWallpaperBlurred(
    ash::mojom::WallpaperController::IsWallpaperBlurredCallback callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::IsActiveUserWallpaperControlledByPolicy(
    ash::mojom::WallpaperController::
        IsActiveUserWallpaperControlledByPolicyCallback callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::GetActiveUserWallpaperInfo(
    ash::mojom::WallpaperController::GetActiveUserWallpaperInfoCallback
        callback) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShouldShowWallpaperSetting(
    ash::mojom::WallpaperController::ShouldShowWallpaperSettingCallback
        callback) {
  NOTIMPLEMENTED();
}
