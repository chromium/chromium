// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/customization/customization_wallpaper_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/wallpaper/wallpaper_drivefs_delegate_impl.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/settings/ash/pref_names.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

using ::ash::ProfileHelper;
using file_manager::VolumeManager;
using session_manager::SessionManager;
using wallpaper_handlers::BackdropSurpriseMeImageFetcher;

namespace {

// Known user keys.
const char kWallpaperFilesId[] = "wallpaper-files-id";

WallpaperControllerClientImpl* g_wallpaper_controller_client_instance = nullptr;

bool IsKnownUser(const AccountId& account_id) {
  return user_manager::UserManager::Get()->IsKnownUser(account_id);
}

// Returns the type of the user with the specified |id| or USER_TYPE_REGULAR.
user_manager::UserType GetUserType(const AccountId& id) {
  if (user_manager::UserManager::IsInitialized()) {
    if (auto* user = user_manager::UserManager::Get()->FindUser(id))
      return user->GetType();
  }
  // TODO(crbug.com/1329256): Convert this to a DCHECK when tests are fixed.
  LOG(WARNING) << "No matching user. This should only happen in tests.";
  // Unit tests may not have a UserManager.
  return user_manager::USER_TYPE_REGULAR;
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
  ash::SystemSaltGetter* salt_getter = ash::SystemSaltGetter::Get();
  DCHECK(salt_getter);

  // System salt must be defined at this point.
  const ash::SystemSaltGetter::RawSalt* salt = salt_getter->GetRawSalt();
  if (!salt)
    LOG(FATAL) << "WallpaperManager HashWallpaperFilesIdStr(): no salt!";

  unsigned char binmd[base::kSHA1Length];
  std::string lowercase(files_id_unhashed);
  base::ranges::transform(lowercase, lowercase.begin(), ::tolower);
  std::vector<uint8_t> data = *salt;
  base::ranges::copy(files_id_unhashed, std::back_inserter(data));
  base::SHA1HashBytes(data.data(), data.size(), binmd);
  std::string result = base::HexEncode(binmd, sizeof(binmd));
  base::ranges::transform(result, result.begin(), ::tolower);
  return result;
}

// Returns true if wallpaper files id can be returned successfully.
bool CanGetFilesId() {
  return ash::SystemSaltGetter::IsInitialized() &&
         ash::SystemSaltGetter::Get()->GetRawSalt();
}

void GetFilesIdSaltReady(
    const AccountId& account_id,
    base::OnceCallback<void(const std::string&)> files_id_callback) {
  DCHECK(CanGetFilesId());
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (const std::string* stored_value =
          known_user.FindStringPath(account_id, kWallpaperFilesId)) {
    std::move(files_id_callback).Run(*stored_value);
    return;
  }

  const std::string wallpaper_files_id =
      HashWallpaperFilesIdStr(account_id.GetUserEmail());
  known_user.SetStringPref(account_id, kWallpaperFilesId, wallpaper_files_id);
  std::move(files_id_callback).Run(wallpaper_files_id);
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

WallpaperControllerClientImpl::WallpaperControllerClientImpl(
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate)
    : wallpaper_fetcher_delegate_(std::move(wallpaper_fetcher_delegate)) {
  local_state_ = g_browser_process->local_state();
  show_user_names_on_signin_subscription_ =
      ash::CrosSettings::Get()->AddSettingsObserver(
          ash::kAccountsPrefShowUserNamesOnSignIn,
          base::BindRepeating(
              &WallpaperControllerClientImpl::ShowWallpaperOnLoginScreen,
              weak_factory_.GetWeakPtr()));

  DCHECK(!g_wallpaper_controller_client_instance);
  g_wallpaper_controller_client_instance = this;

  SessionManager* session_manager = SessionManager::Get();
  // SessionManager might not exist in unit tests.
  if (session_manager)
    session_observation_.Observe(session_manager);
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

void WallpaperControllerClientImpl::SetWallpaperFetcherDelegateForTesting(
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate) {
  wallpaper_fetcher_delegate_.swap(wallpaper_fetcher_delegate);
}

void WallpaperControllerClientImpl::SetInitialWallpaper() {
  // Apply device customization.
  namespace customization_util = ash::customization_wallpaper_util;
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
          ash::switches::kGuestSession)) {
    return;
  }

  // Do not set wallpaper in tests.
  if (ash::WizardController::IsZeroDelayEnabled())
    return;

  // Show the wallpaper of the active user during an user session.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    ShowUserWallpaper(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
    return;
  }

  // Show a wallpaper during OOBE.
  if (SessionManager::Get()->session_state() ==
      session_manager::SessionState::OOBE) {
    wallpaper_controller_->ShowOobeWallpaper();
    return;
  }

  ShowWallpaperOnLoginScreen();
}

// static
WallpaperControllerClientImpl* WallpaperControllerClientImpl::Get() {
  return g_wallpaper_controller_client_instance;
}

void WallpaperControllerClientImpl::SetOnlineWallpaper(
    const ash::OnlineWallpaperParams& params,
    ash::WallpaperController::SetWallpaperCallback callback) {
  if (!IsKnownUser(params.account_id))
    return;

  wallpaper_controller_->SetOnlineWallpaper(params, std::move(callback));
}

void WallpaperControllerClientImpl::SetGooglePhotosWallpaper(
    const ash::GooglePhotosWallpaperParams& params,
    ash::WallpaperController::SetWallpaperCallback callback) {
  if (!IsKnownUser(params.account_id))
    return;

  wallpaper_controller_->SetGooglePhotosWallpaper(params, std::move(callback));
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

  wallpaper_controller_->SetPolicyWallpaper(account_id, GetUserType(account_id),
                                            *data);
}

bool WallpaperControllerClientImpl::SetThirdPartyWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image) {
  RecordWallpaperSourceUMA(ash::WallpaperType::kThirdParty);
  return IsKnownUser(account_id) &&
         wallpaper_controller_->SetThirdPartyWallpaper(account_id, file_name,
                                                       layout, image);
}

void WallpaperControllerClientImpl::ShowUserWallpaper(
    const AccountId& account_id) {
  if (IsKnownUser(account_id)) {
    user_manager::UserType user_type = GetUserType(account_id);
    wallpaper_controller_->ShowUserWallpaper(account_id, user_type);
  }
}

void WallpaperControllerClientImpl::ShowSigninWallpaper() {
  wallpaper_controller_->ShowSigninWallpaper();
}

void WallpaperControllerClientImpl::ShowOverrideWallpaper(
    const base::FilePath& image_path,
    bool always_on_top) {
  wallpaper_controller_->ShowOverrideWallpaper(image_path, always_on_top);
}

void WallpaperControllerClientImpl::RemoveOverrideWallpaper() {
  wallpaper_controller_->RemoveOverrideWallpaper();
}

void WallpaperControllerClientImpl::RemoveUserWallpaper(
    const AccountId& account_id,
    base::OnceClosure on_removed) {
  if (!IsKnownUser(account_id)) {
    std::move(on_removed).Run();
    return;
  }

  wallpaper_controller_->RemoveUserWallpaper(account_id, std::move(on_removed));
}

void WallpaperControllerClientImpl::RemovePolicyWallpaper(
    const AccountId& account_id) {
  if (!IsKnownUser(account_id))
    return;

  wallpaper_controller_->RemovePolicyWallpaper(account_id);
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

absl::optional<ash::WallpaperInfo>
WallpaperControllerClientImpl::GetActiveUserWallpaperInfo() {
  return wallpaper_controller_->GetActiveUserWallpaperInfo();
}

void WallpaperControllerClientImpl::GetFilesId(
    const AccountId& account_id,
    base::OnceCallback<void(const std::string&)> files_id_callback) const {
  ash::SystemSaltGetter::Get()->AddOnSystemSaltReady(base::BindOnce(
      &GetFilesIdSaltReady, account_id, std::move(files_id_callback)));
}

bool WallpaperControllerClientImpl::IsWallpaperSyncEnabled(
    const AccountId& account_id) const {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!profile)
    return false;

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service)
    return false;
  syncer::SyncUserSettings* user_settings = sync_service->GetUserSettings();
  return user_settings->IsSyncAllOsTypesEnabled() ||
         profile->GetPrefs()->GetBoolean(
             ash::settings::prefs::kSyncOsWallpaper);
}

void WallpaperControllerClientImpl::OnVolumeMounted(
    ash::MountError error_code,
    const file_manager::Volume& volume) {
  if (error_code != ash::MountError::kSuccess) {
    return;
  }
  if (volume.type() != file_manager::VolumeType::VOLUME_TYPE_GOOGLE_DRIVE) {
    return;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  VolumeManager* volume_manager = VolumeManager::Get(profile);
  // Volume ID is based on the mount path, which for drive is based on the
  // account_id.
  if (!volume_manager->FindVolumeById(volume.volume_id())) {
    return;
  }
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  CHECK(user);
  wallpaper_controller_->SyncLocalAndRemotePrefs(user->GetAccountId());
}

void WallpaperControllerClientImpl::OnUserProfileLoaded(
    const AccountId& account_id) {
  wallpaper_controller_->SyncLocalAndRemotePrefs(account_id);
  ObserveVolumeManagerForAccountId(account_id);
}

void WallpaperControllerClientImpl::DeviceWallpaperImageFilePathChanged() {
  wallpaper_controller_->SetDevicePolicyWallpaperPath(
      GetDeviceWallpaperImageFilePath());
}

void WallpaperControllerClientImpl::InitController() {
  wallpaper_controller_->SetClient(this);
  wallpaper_controller_->SetDriveFsDelegate(
      std::make_unique<ash::WallpaperDriveFsDelegateImpl>());

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
  ash::SystemAppLaunchParams params;
  params.url = GURL(
      std::string(ash::personalization_app::kChromeUIPersonalizationAppURL) +
      ash::personalization_app::kWallpaperSubpageRelativeUrl);
  params.launch_source = apps::LaunchSource::kFromShelf;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::PERSONALIZATION,
                               params);
}

void WallpaperControllerClientImpl::SetDefaultWallpaper(
    const AccountId& account_id,
    bool show_wallpaper,
    ash::WallpaperController::SetWallpaperCallback callback) {
  if (!IsKnownUser(account_id))
    return;

  wallpaper_controller_->SetDefaultWallpaper(account_id, show_wallpaper,
                                             std::move(callback));
}

void WallpaperControllerClientImpl::FetchDailyRefreshWallpaper(
    const std::string& collection_id,
    DailyWallpaperUrlFetchedCallback callback) {
  surprise_me_image_fetcher_ = std::make_unique<BackdropSurpriseMeImageFetcher>(
      collection_id, /*resume_token=*/std::string());
  surprise_me_image_fetcher_->Start(
      base::BindOnce(&WallpaperControllerClientImpl::OnDailyImageInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WallpaperControllerClientImpl::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  auto images_info_fetcher =
      wallpaper_fetcher_delegate_->CreateBackdropImageInfoFetcher(
          collection_id);
  auto* images_info_fetcher_ptr = images_info_fetcher.get();
  images_info_fetcher_ptr->Start(
      base::BindOnce(&WallpaperControllerClientImpl::OnFetchImagesForCollection,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(images_info_fetcher)));
}

void WallpaperControllerClientImpl::FetchGooglePhotosPhoto(
    const AccountId& account_id,
    const std::string& id,
    FetchGooglePhotosPhotoCallback callback) {
  if (google_photos_photos_fetchers_.find(account_id) ==
      google_photos_photos_fetchers_.end()) {
    Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
    google_photos_photos_fetchers_.insert(
        {account_id,
         wallpaper_fetcher_delegate_->CreateGooglePhotosPhotosFetcher(
             profile)});
  }
  auto fetched_callback =
      base::BindOnce(&WallpaperControllerClientImpl::OnGooglePhotosPhotoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  google_photos_photos_fetchers_[account_id]->AddRequestAndStartIfNecessary(
      id, /*album_id=*/absl::nullopt,
      /*resume_token=*/absl::nullopt, /*shuffle=*/false,
      std::move(fetched_callback));
}

void WallpaperControllerClientImpl::FetchDailyGooglePhotosPhoto(
    const AccountId& account_id,
    const std::string& album_id,
    FetchGooglePhotosPhotoCallback callback) {
  if (google_photos_photos_fetchers_.find(account_id) ==
      google_photos_photos_fetchers_.end()) {
    Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
    google_photos_photos_fetchers_.insert(
        {account_id,
         wallpaper_fetcher_delegate_->CreateGooglePhotosPhotosFetcher(
             profile)});
  }
  auto fetched_callback = base::BindOnce(
      &WallpaperControllerClientImpl::OnGooglePhotosDailyAlbumFetched,
      weak_factory_.GetWeakPtr(), account_id, std::move(callback));
  google_photos_photos_fetchers_[account_id]->AddRequestAndStartIfNecessary(
      /*item_id=*/absl::nullopt, album_id,
      /*resume_token=*/absl::nullopt, /*shuffle=*/true,
      std::move(fetched_callback));
}

void WallpaperControllerClientImpl::FetchGooglePhotosAccessToken(
    const AccountId& account_id,
    FetchGooglePhotosAccessTokenCallback callback) {
  wallpaper_fetcher_delegate_->FetchGooglePhotosAccessToken(
      account_id, std::move(callback));
}

bool WallpaperControllerClientImpl::ShouldShowUserNamesOnLogin() const {
  bool show_user_names = true;
  ash::CrosSettings::Get()->GetBoolean(ash::kAccountsPrefShowUserNamesOnSignIn,
                                       &show_user_names);
  return show_user_names;
}

base::FilePath
WallpaperControllerClientImpl::GetDeviceWallpaperImageFilePath() {
  return base::FilePath(
      local_state_->GetString(prefs::kDeviceWallpaperImageFilePath));
}

void WallpaperControllerClientImpl::OnDailyImageInfoFetched(
    DailyWallpaperUrlFetchedCallback callback,
    bool success,
    const backdrop::Image& image,
    const std::string& next_resume_token) {
  std::move(callback).Run(success, std::move(image));
  surprise_me_image_fetcher_.reset();
}

void WallpaperControllerClientImpl::OnFetchImagesForCollection(
    FetchImagesForCollectionCallback callback,
    std::unique_ptr<wallpaper_handlers::BackdropImageInfoFetcher> fetcher,
    bool success,
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  std::move(callback).Run(success, std::move(images));
}

void WallpaperControllerClientImpl::OnGooglePhotosPhotoFetched(
    FetchGooglePhotosPhotoCallback callback,
    ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr
        response) {
  // If we have a `GooglePhotosPhoto`, pass that along. Otherwise, indicate to
  // `callback` whether the the request succeeded or failed, since `callback`
  // can take action if the `GooglePhotosPhoto` with the given id has been
  // deleted.
  if (response->photos.has_value() && response->photos.value().size() == 1) {
    std::move(callback).Run(std::move(response->photos.value()[0]),
                            /*success=*/true);
  } else {
    std::move(callback).Run(nullptr, /*success=*/response->photos.has_value());
  }
}

void WallpaperControllerClientImpl::OnGooglePhotosDailyAlbumFetched(
    const AccountId& account_id,
    FetchGooglePhotosPhotoCallback callback,
    ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr
        response) {
  if (!response->photos.has_value() || response->photos.value().size() == 0) {
    std::move(callback).Run(nullptr, /*success=*/response->photos.has_value());
    return;
  }

  if (response->photos.value().size() == 1) {
    std::move(callback).Run(std::move(response->photos.value()[0]),
                            /*success=*/true);
    return;
  }

  // For small albums (n<12), we will repeat the photos in the same shuffled
  // order indefinitely as we cycle through the cache. For large albums, the
  // cache of size 10 makes sure we don't repeat photos within 10 refreshes.
  auto& photos = response->photos.value();
  int new_size = std::min(10, static_cast<int>(photos.size() - 1));

  // Using a cache with the new size to populate with the stored data means
  // that we will inherently drop the oldest values if the album has shrunk
  // enough to warrant a change in cache size.
  ash::WallpaperController::DailyGooglePhotosIdCache ids(new_size);
  if (!wallpaper_controller_->GetDailyGooglePhotosWallpaperIdCache(account_id,
                                                                   ids)) {
    // This is expected the first time a user uses Google Photos wallpaper, but
    // would be an error after that.
    DVLOG(1) << "No cache of previously shown Google Photos ids found."
                "Starting with an empty one.";
  }

  // TODO(b/229146895): Delete this shuffle once the Google Photos API has
  // been updated to shuffle for us. This doesn't work for big albums (n>100).
  base::RandomShuffle(photos.begin(), photos.end());

  // Get the first photo from the shuffled set that is not in the LRU cache.
  auto selected_itr = base::ranges::find_if(
      photos,
      [&ids](
          const ash::personalization_app::mojom::GooglePhotosPhotoPtr& photo) {
        return ids.Peek(base::PersistentHash(photo->id)) == ids.end();
      });

  DCHECK(selected_itr != photos.end());
  auto& selected = *selected_itr;

  ids.Put(base::PersistentHash(selected->id));
  bool success = wallpaper_controller_->SetDailyGooglePhotosWallpaperIdCache(
      account_id, ids);
  DCHECK(success);
  std::move(callback).Run(std::move(selected),
                          /*success=*/true);
}

void WallpaperControllerClientImpl::ObserveVolumeManagerForAccountId(
    const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  CHECK(profile);
  volume_manager_observation_.AddObservation(VolumeManager::Get(profile));
}

void WallpaperControllerClientImpl::RecordWallpaperSourceUMA(
    const ash::WallpaperType type) {
  base::UmaHistogramEnumeration("Ash.Wallpaper.Source2", type,
                                ash::WallpaperType::kCount);
}
