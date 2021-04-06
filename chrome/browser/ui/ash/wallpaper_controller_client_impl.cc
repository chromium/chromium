// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/customization/customization_wallpaper_util.h"
#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace {

// Known user keys.
const char kWallpaperFilesId[] = "wallpaper-files-id";

WallpaperControllerClientImpl* g_wallpaper_controller_client_instance = nullptr;

bool IsKnownUser(const AccountId& account_id) {
  return user_manager::UserManager::Get()->IsKnownUser(account_id);
}

// This has once been copied from
// brillo::cryptohome::home::SanitizeUserName(username) to be used for
// wallpaper identification purpose only.
//
// Historic note: We need some way to identify users wallpaper files in
// the device filesystem. Historically User::username_hash() was used for this
// purpose, but it has two caveats:
// 1. username_hash() is defined only after user has logged in.
// 2. If cryptohome identifier changes, username_hash() will also change,
//    and we may lose user => wallpaper files mapping at that point.
// So this function gives WallpaperManager independent hashing method to break
// this dependency.
std::string HashWallpaperFilesIdStr(const std::string& files_id_unhashed) {
  chromeos::SystemSaltGetter* salt_getter = chromeos::SystemSaltGetter::Get();
  DCHECK(salt_getter);

  // System salt must be defined at this point.
  const chromeos::SystemSaltGetter::RawSalt* salt = salt_getter->GetRawSalt();
  if (!salt)
    LOG(FATAL) << "WallpaperManager HashWallpaperFilesIdStr(): no salt!";

  unsigned char binmd[base::kSHA1Length];
  std::string lowercase(files_id_unhashed);
  std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                 ::tolower);
  std::vector<uint8_t> data = *salt;
  std::copy(files_id_unhashed.begin(), files_id_unhashed.end(),
            std::back_inserter(data));
  base::SHA1HashBytes(data.data(), data.size(), binmd);
  std::string result = base::HexEncode(binmd, sizeof(binmd));
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

// Returns true if wallpaper files id can be returned successfully.
bool CanGetFilesId() {
  return chromeos::SystemSaltGetter::IsInitialized() &&
         chromeos::SystemSaltGetter::Get()->GetRawSalt();
}

// Calls |callback| when system salt is ready. (|CanGetFilesId| returns true.)
void AddCanGetFilesIdCallback(base::OnceClosure callback) {
  // System salt may not be initialized in tests.
  if (chromeos::SystemSaltGetter::IsInitialized()) {
    chromeos::SystemSaltGetter::Get()->AddOnSystemSaltReady(
        std::move(callback));
  }
}

// Returns true if |users| contains users other than device local accounts.
bool HasNonDeviceLocalAccounts(const user_manager::UserList& users) {
  for (const user_manager::User* user : users) {
    if (!policy::IsDeviceLocalAccountUser(user->GetAccountId().GetUserEmail(),
                                          nullptr))
      return true;
  }
  return false;
}

// Returns the first public session user found in |users|, or null if there's
// none.
user_manager::User* FindPublicSession(const user_manager::UserList& users) {
  for (size_t i = 0; i < users.size(); ++i) {
    if (users[i]->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
      return users[i];
  }
  return nullptr;
}

}  // namespace

WallpaperControllerClientImpl::WallpaperControllerClientImpl() {
  local_state_ = g_browser_process->local_state();
  show_user_names_on_signin_subscription_ =
      ash::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kAccountsPrefShowUserNamesOnSignIn,
          base::BindRepeating(
              &WallpaperControllerClientImpl::ShowWallpaperOnLoginScreen,
              weak_factory_.GetWeakPtr()));

  DCHECK(!g_wallpaper_controller_client_instance);
  g_wallpaper_controller_client_instance = this;
}

WallpaperControllerClientImpl::~WallpaperControllerClientImpl() {
  wallpaper_controller_->SetClient(nullptr);
  weak_factory_.InvalidateWeakPtrs();
  DCHECK_EQ(this, g_wallpaper_controller_client_instance);
  g_wallpaper_controller_client_instance = nullptr;
}

void WallpaperControllerClientImpl::Init() {
  pref_registrar_.Init(local_state_);
  pref_registrar_.Add(
      prefs::kDeviceWallpaperImageFilePath,
      base::BindRepeating(
          &WallpaperControllerClientImpl::DeviceWallpaperImageFilePathChanged,
          weak_factory_.GetWeakPtr()));
  wallpaper_controller_ = ash::WallpaperController::Get();
  InitController();
}

void WallpaperControllerClientImpl::InitForTesting(
    ash::WallpaperController* controller) {
  wallpaper_controller_ = controller;
  InitController();
}

void WallpaperControllerClientImpl::SetInitialWallpaper() {
  // Apply device customization.
  namespace customization_util = chromeos::customization_wallpaper_util;
  if (customization_util::ShouldUseCustomizedDefaultWallpaper()) {
    base::FilePath customized_default_small_path;
    base::FilePath customized_default_large_path;
    if (customization_util::GetCustomizedDefaultWallpaperPaths(
            &customized_default_small_path, &customized_default_large_path)) {
      wallpaper_controller_->SetCustomizedDefaultWallpaperPaths(
          customized_default_small_path, customized_default_large_path);
    }
  }

  // Guest wallpaper should be initialized when guest logs in.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kGuestSession)) {
    return;
  }

  // Do not set wallpaper in tests.
  if (chromeos::WizardController::IsZeroDelayEnabled())
    return;

  // Show the wallpaper of the active user during an user session.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    ShowUserWallpaper(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
    return;
  }

  // Show a white wallpaper during OOBE.
  if (session_manager::SessionManager::Get()->session_state() ==
      session_manager::SessionState::OOBE) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(SK_ColorWHITE);
    wallpaper_controller_->ShowOneShotWallpaper(
        gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    return;
  }

  ShowWallpaperOnLoginScreen();
}

// static
WallpaperControllerClientImpl* WallpaperControllerClientImpl::Get() {
  return g_wallpaper_controller_client_instance;
}

std::string WallpaperControllerClientImpl::GetFilesId(
    const AccountId& account_id) const {
  DCHECK(CanGetFilesId());
  std::string stored_value;
  if (user_manager::known_user::GetStringPref(account_id, kWallpaperFilesId,
                                              &stored_value)) {
    return stored_value;
  }

  const std::string wallpaper_files_id =
      HashWallpaperFilesIdStr(account_id.GetUserEmail());
  user_manager::known_user::SetStringPref(account_id, kWallpaperFilesId,
                                          wallpaper_files_id);
  return wallpaper_files_id;
}

void WallpaperControllerClientImpl::SetCustomWallpaper(
    const AccountId& account_id,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image,
    bool preview_mode) {
  if (!IsKnownUser(account_id))
    return;
  wallpaper_controller_->SetCustomWallpaper(
      account_id, wallpaper_files_id, file_name, layout, image, preview_mode);
}

void WallpaperControllerClientImpl::SetOnlineWallpaperIfExists(
    const AccountId& account_id,
    const std::string& url,
    ash::WallpaperLayout layout,
    bool preview_mode,
    ash::WallpaperController::SetOnlineWallpaperIfExistsCallback callback) {
  if (!IsKnownUser(account_id))
    return;
  wallpaper_controller_->SetOnlineWallpaperIfExists(
      account_id, url, layout, preview_mode, std::move(callback));
}

void WallpaperControllerClientImpl::SetOnlineWallpaperFromData(
    const AccountId& account_id,
    const std::string& image_data,
    const std::string& url,
    ash::WallpaperLayout layout,
    bool preview_mode,
    ash::WallpaperController::SetOnlineWallpaperFromDataCallback callback) {
  if (!IsKnownUser(account_id))
    return;
  wallpaper_controller_->SetOnlineWallpaperFromData(
      account_id, image_data, url, layout, preview_mode, std::move(callback));
}

void WallpaperControllerClientImpl::SetCustomizedDefaultWallpaperPaths(
    const base::FilePath& customized_default_small_path,
    const base::FilePath& customized_default_large_path) {
  wallpaper_controller_->SetCustomizedDefaultWallpaperPaths(
      customized_default_small_path, customized_default_large_path);
}

void WallpaperControllerClientImpl::SetPolicyWallpaper(
    const AccountId& account_id,
    std::unique_ptr<std::string> data) {
  if (!data || !IsKnownUser(account_id))
    return;

  // Postpone setting the wallpaper until we can get files id. See
  // https://crbug.com/615239.
  if (!CanGetFilesId()) {
    AddCanGetFilesIdCallback(base::BindOnce(
        &WallpaperControllerClientImpl::SetPolicyWallpaper,
        weak_factory_.GetWeakPtr(), account_id, std::move(data)));
    return;
  }

  wallpaper_controller_->SetPolicyWallpaper(account_id, GetFilesId(account_id),
                                            *data);
}

bool WallpaperControllerClientImpl::SetThirdPartyWallpaper(
    const AccountId& account_id,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image) {
  return IsKnownUser(account_id) &&
         wallpaper_controller_->SetThirdPartyWallpaper(
             account_id, wallpaper_files_id, file_name, layout, image);
}

void WallpaperControllerClientImpl::ConfirmPreviewWallpaper() {
  wallpaper_controller_->ConfirmPreviewWallpaper();
}

void WallpaperControllerClientImpl::CancelPreviewWallpaper() {
  wallpaper_controller_->CancelPreviewWallpaper();
}

void WallpaperControllerClientImpl::UpdateCustomWallpaperLayout(
    const AccountId& account_id,
    ash::WallpaperLayout layout) {
  if (IsKnownUser(account_id))
    wallpaper_controller_->UpdateCustomWallpaperLayout(account_id, layout);
}

void WallpaperControllerClientImpl::ShowUserWallpaper(
    const AccountId& account_id) {
  if (IsKnownUser(account_id))
    wallpaper_controller_->ShowUserWallpaper(account_id);
}

void WallpaperControllerClientImpl::ShowSigninWallpaper() {
  wallpaper_controller_->ShowSigninWallpaper();
}

void WallpaperControllerClientImpl::ShowAlwaysOnTopWallpaper(
    const base::FilePath& image_path) {
  wallpaper_controller_->ShowAlwaysOnTopWallpaper(image_path);
}

void WallpaperControllerClientImpl::RemoveAlwaysOnTopWallpaper() {
  wallpaper_controller_->RemoveAlwaysOnTopWallpaper();
}

void WallpaperControllerClientImpl::RemoveUserWallpaper(
    const AccountId& account_id) {
  if (!IsKnownUser(account_id))
    return;

  // Postpone removing the wallpaper until we can get files id.
  if (!CanGetFilesId()) {
    LOG(WARNING)
        << "Cannot get wallpaper files id in RemoveUserWallpaper. This "
           "should never happen under normal circumstances.";
    AddCanGetFilesIdCallback(
        base::BindOnce(&WallpaperControllerClientImpl::RemoveUserWallpaper,
                       weak_factory_.GetWeakPtr(), account_id));
    return;
  }

  wallpaper_controller_->RemoveUserWallpaper(account_id,
                                             GetFilesId(account_id));
}

void WallpaperControllerClientImpl::RemovePolicyWallpaper(
    const AccountId& account_id) {
  if (!IsKnownUser(account_id))
    return;

  // Postpone removing the wallpaper until we can get files id.
  if (!CanGetFilesId()) {
    LOG(WARNING)
        << "Cannot get wallpaper files id in RemovePolicyWallpaper. This "
           "should never happen under normal circumstances.";
    AddCanGetFilesIdCallback(
        base::BindOnce(&WallpaperControllerClientImpl::RemovePolicyWallpaper,
                       weak_factory_.GetWeakPtr(), account_id));
    return;
  }

  wallpaper_controller_->RemovePolicyWallpaper(account_id,
                                               GetFilesId(account_id));
}

void WallpaperControllerClientImpl::GetOfflineWallpaperList(
    ash::WallpaperController::GetOfflineWallpaperListCallback callback) {
  wallpaper_controller_->GetOfflineWallpaperList(std::move(callback));
}

void WallpaperControllerClientImpl::SetAnimationDuration(
    const base::TimeDelta& animation_duration) {
  wallpaper_controller_->SetAnimationDuration(animation_duration);
}

void WallpaperControllerClientImpl::OpenWallpaperPickerIfAllowed() {
  wallpaper_controller_->OpenWallpaperPickerIfAllowed();
}

void WallpaperControllerClientImpl::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  wallpaper_controller_->MinimizeInactiveWindows(user_id_hash);
}

void WallpaperControllerClientImpl::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  wallpaper_controller_->RestoreMinimizedWindows(user_id_hash);
}

void WallpaperControllerClientImpl::AddObserver(
    ash::WallpaperControllerObserver* observer) {
  wallpaper_controller_->AddObserver(observer);
}

void WallpaperControllerClientImpl::RemoveObserver(
    ash::WallpaperControllerObserver* observer) {
  wallpaper_controller_->RemoveObserver(observer);
}

gfx::ImageSkia WallpaperControllerClientImpl::GetWallpaperImage() {
  return wallpaper_controller_->GetWallpaperImage();
}

const std::vector<SkColor>&
WallpaperControllerClientImpl::GetWallpaperColors() {
  return wallpaper_controller_->GetWallpaperColors();
}

bool WallpaperControllerClientImpl::IsWallpaperBlurred() {
  return wallpaper_controller_->IsWallpaperBlurredForLockState();
}

bool WallpaperControllerClientImpl::IsActiveUserWallpaperControlledByPolicy() {
  return wallpaper_controller_->IsActiveUserWallpaperControlledByPolicy();
}

ash::WallpaperInfo WallpaperControllerClientImpl::GetActiveUserWallpaperInfo() {
  return wallpaper_controller_->GetActiveUserWallpaperInfo();
}

bool WallpaperControllerClientImpl::ShouldShowWallpaperSetting() {
  return wallpaper_controller_->ShouldShowWallpaperSetting();
}

void WallpaperControllerClientImpl::DeviceWallpaperImageFilePathChanged() {
  wallpaper_controller_->SetDevicePolicyWallpaperPath(
      GetDeviceWallpaperImageFilePath());
}

void WallpaperControllerClientImpl::InitController() {
  wallpaper_controller_->SetClient(this);

  base::FilePath user_data;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data));
  base::FilePath wallpapers;
  CHECK(base::PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpapers));
  base::FilePath custom_wallpapers;
  CHECK(base::PathService::Get(chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS,
                               &custom_wallpapers));
  base::FilePath device_policy_wallpaper = GetDeviceWallpaperImageFilePath();
  wallpaper_controller_->Init(user_data, wallpapers, custom_wallpapers,
                              device_policy_wallpaper);
}

void WallpaperControllerClientImpl::ShowWallpaperOnLoginScreen() {
  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    return;

  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  user_manager::User* public_session = FindPublicSession(users);

  // Show the default signin wallpaper if there's no user to display.
  if ((!ShouldShowUserNamesOnLogin() && !public_session) ||
      !HasNonDeviceLocalAccounts(users)) {
    ShowSigninWallpaper();
    return;
  }

  // Normal boot, load user wallpaper.
  const AccountId account_id = public_session ? public_session->GetAccountId()
                                              : users[0]->GetAccountId();
  ShowUserWallpaper(account_id);
}

void WallpaperControllerClientImpl::OpenWallpaperPicker() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  if (ash::features::IsWallpaperWebUIEnabled()) {
    web_app::LaunchSystemWebAppAsync(profile,
                                     web_app::SystemAppType::PERSONALIZATION);
    return;
  }

  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  if (proxy->AppRegistryCache().GetAppType(
          extension_misc::kWallpaperManagerId) ==
      apps::mojom::AppType::kUnknown) {
    return;
  }
  proxy->Launch(
      extension_misc::kWallpaperManagerId,
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          false /* preferred_containner */),
      apps::mojom::LaunchSource::kFromChromeInternal);
}

void WallpaperControllerClientImpl::MaybeClosePreviewWallpaper() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);

  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);

  auto event_args = std::make_unique<base::ListValue>();
  auto event = std::make_unique<extensions::Event>(
      extensions::events::WALLPAPER_PRIVATE_ON_CLOSE_PREVIEW_WALLPAPER,
      extensions::api::wallpaper_private::OnClosePreviewWallpaper::kEventName,
      std::move(event_args));
  event_router->DispatchEventToExtension(extension_misc::kWallpaperManagerId,
                                         std::move(event));
}

void WallpaperControllerClientImpl::SetDefaultWallpaper(
    const AccountId& account_id,
    bool show_wallpaper) {
  if (!IsKnownUser(account_id))
    return;

  // Postpone setting the wallpaper until we can get files id.
  if (!CanGetFilesId()) {
    LOG(WARNING)
        << "Cannot get wallpaper files id in SetDefaultWallpaper. This "
           "should never happen under normal circumstances.";
    AddCanGetFilesIdCallback(
        base::BindOnce(&WallpaperControllerClient::SetDefaultWallpaper,
                       weak_factory_.GetWeakPtr(), account_id, show_wallpaper));
    return;
  }

  wallpaper_controller_->SetDefaultWallpaper(account_id, GetFilesId(account_id),
                                             show_wallpaper);
}

bool WallpaperControllerClientImpl::ShouldShowUserNamesOnLogin() const {
  bool show_user_names = true;
  ash::CrosSettings::Get()->GetBoolean(
      chromeos::kAccountsPrefShowUserNamesOnSignIn, &show_user_names);
  return show_user_names;
}

base::FilePath
WallpaperControllerClientImpl::GetDeviceWallpaperImageFilePath() {
  return base::FilePath(
      local_state_->GetString(prefs::kDeviceWallpaperImageFilePath));
}
