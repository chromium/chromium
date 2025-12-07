// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "crypto/hmac.h"
#include "crypto/random.h"

namespace {

void LogGetHmacKeyResult(AppShimRegistry::GetHmacKeyResult result) {
  base::UmaHistogramEnumeration("Apps.AppShimRegistry.HmacKeyStore.LoadResult",
                                result);
}

void LogSaveHmacKeyResult(AppShimRegistry::SaveHmacKeyResult result) {
  base::UmaHistogramEnumeration("Apps.AppShimRegistry.HmacKeyStore.SaveResult",
                                result);
}

const char kAppShims[] = "app_shims";
const char kAppShimsCdHashHmacKey[] = "app_shims_cdhash_hmac_key";
const char kInstalledProfiles[] = "installed_profiles";
const char kLastActiveProfiles[] = "last_active_profiles";
const char kHandlers[] = "handlers";
const char kFileHandlerExtensions[] = "extensions";
const char kFileHandlerMimeTypes[] = "mime_types";
const char kProtocolHandlers[] = "protocols";
const char kCdHashHmac[] = "cdhash_hmac";
const char kNotificationPermissionStatus[] = "notification_permission";

base::Value::List SetToValueList(const std::set<std::string>& values) {
  base::Value::List result;
  for (const auto& s : values) {
    result.Append(s);
  }
  return result;
}

std::set<std::string> ValueListToSet(const base::Value::List* list) {
  std::set<std::string> result;
  if (list) {
    for (const auto& v : *list) {
      if (!v.is_string())
        continue;
      result.insert(v.GetString());
    }
  }
  return result;
}

}  // namespace

AppShimRegistry::HandlerInfo::HandlerInfo() = default;
AppShimRegistry::HandlerInfo::~HandlerInfo() = default;
AppShimRegistry::HandlerInfo::HandlerInfo(HandlerInfo&&) = default;
AppShimRegistry::HandlerInfo::HandlerInfo(const HandlerInfo&) = default;
AppShimRegistry::HandlerInfo& AppShimRegistry::HandlerInfo::operator=(
    HandlerInfo&&) = default;
AppShimRegistry::HandlerInfo& AppShimRegistry::HandlerInfo::operator=(
    const HandlerInfo&) = default;

// static
AppShimRegistry* AppShimRegistry::Get() {
  static base::NoDestructor<AppShimRegistry> instance;
  return instance.get();
}

void AppShimRegistry::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kAppShims);
  registry->RegisterStringPref(kAppShimsCdHashHmacKey, "");
}

std::set<base::FilePath> AppShimRegistry::GetInstalledProfilesForApp(
    const std::string& app_id) const {
  std::set<base::FilePath> installed_profiles;
  GetProfilesSetForApp(app_id, kInstalledProfiles, &installed_profiles);
  return installed_profiles;
}

bool AppShimRegistry::IsAppInstalledInProfile(
    const std::string& app_id,
    const base::FilePath& profile) const {
  return GetInstalledProfilesForApp(app_id).contains(profile);
}

std::set<base::FilePath> AppShimRegistry::GetLastActiveProfilesForApp(
    const std::string& app_id) const {
  std::set<base::FilePath> last_active_profiles;
  GetProfilesSetForApp(app_id, kLastActiveProfiles, &last_active_profiles);

  // Cull out any profiles that are not installed.
  std::set<base::FilePath> installed_profiles;
  GetProfilesSetForApp(app_id, kInstalledProfiles, &installed_profiles);
  for (auto it = last_active_profiles.begin();
       it != last_active_profiles.end();) {
    if (installed_profiles.count(*it))
      it++;
    else
      last_active_profiles.erase(it++);
  }
  return last_active_profiles;
}

void AppShimRegistry::GetProfilesSetForApp(
    const std::string& app_id,
    const std::string& profiles_key,
    std::set<base::FilePath>* profiles) const {
  PrefService* pref_service = GetPrefService();
  CHECK(pref_service);
  const base::Value::Dict& cache = pref_service->GetDict(kAppShims);
  const base::Value::Dict* app_info = cache.FindDict(app_id);
  if (!app_info)
    return;
  const base::Value::List* profile_values = app_info->FindList(profiles_key);
  if (!profile_values)
    return;
  for (const auto& profile_path_value : *profile_values) {
    if (profile_path_value.is_string())
      profiles->insert(GetFullProfilePath(profile_path_value.GetString()));
  }
}

void AppShimRegistry::OnAppInstalledForProfile(const std::string& app_id,
                                               const base::FilePath& profile) {
  std::set<base::FilePath> installed_profiles =
      GetInstalledProfilesForApp(app_id);
  if (installed_profiles.count(profile))
    return;
  installed_profiles.insert(profile);
  // Also add the profile to the last active profiles. This way the next time
  // the app is launched, it will at least launch in the most recently
  // installed profile.
  std::set<base::FilePath> last_active_profiles =
      GetLastActiveProfilesForApp(app_id);
  last_active_profiles.insert(profile);
  SetAppInfo(app_id, &installed_profiles, &last_active_profiles,
             /*handlers=*/nullptr, /*cd_hash_hmac_base64=*/nullptr,
             /*notification_permission_status=*/nullptr);
}

bool AppShimRegistry::OnAppUninstalledForProfile(
    const std::string& app_id,
    const base::FilePath& profile) {
  auto installed_profiles = GetInstalledProfilesForApp(app_id);
  auto found = installed_profiles.find(profile);
  if (found != installed_profiles.end()) {
    installed_profiles.erase(profile);
    SetAppInfo(app_id, &installed_profiles, /*last_active_profiles=*/nullptr,
               /*handlers=*/nullptr, /*cd_hash_hmac_base64=*/nullptr,
               /*notification_permission_status=*/nullptr);
  }
  return installed_profiles.empty();
}

void AppShimRegistry::SaveLastActiveProfilesForApp(
    const std::string& app_id,
    std::set<base::FilePath> last_active_profiles) {
  SetAppInfo(app_id, /*installed_profiles=*/nullptr, &last_active_profiles,
             /*handlers=*/nullptr, /*cd_hash_hmac_base64=*/nullptr,
             /*notification_permission_status=*/nullptr);
}

std::set<std::string> AppShimRegistry::GetInstalledAppsForProfile(
    const base::FilePath& profile) const {
  std::set<std::string> result;
  const base::Value::Dict& app_shims = GetPrefService()->GetDict(kAppShims);
  for (const auto iter_app : app_shims) {
    const base::Value::List* installed_profiles_list =
        iter_app.second.GetDict().FindList(kInstalledProfiles);
    if (!installed_profiles_list)
      continue;
    for (const auto& profile_path_value : *installed_profiles_list) {
      if (!profile_path_value.is_string())
        continue;
      if (profile == GetFullProfilePath(profile_path_value.GetString())) {
        result.insert(iter_app.first);
        break;
      }
    }
  }
  return result;
}

std::set<std::string> AppShimRegistry::GetAppsInstalledInMultipleProfiles()
    const {
  std::set<std::string> result;
  if (!GetPrefService()) {
    return result;
  }
  const base::Value::Dict& app_shims = GetPrefService()->GetDict(kAppShims);
  for (const auto iter_app : app_shims) {
    const base::Value::List* installed_profiles_list =
        iter_app.second.GetDict().FindList(kInstalledProfiles);
    if (!installed_profiles_list || installed_profiles_list->size() <= 1) {
      continue;
    }
    result.insert(iter_app.first);
  }
  return result;
}

void AppShimRegistry::SaveFileHandlersForAppAndProfile(
    const std::string& app_id,
    const base::FilePath& profile,
    std::set<std::string> file_handler_extensions,
    std::set<std::string> file_handler_mime_types) {
  std::map<base::FilePath, HandlerInfo> handlers = GetHandlersForApp(app_id);
  auto it = handlers.emplace(profile, HandlerInfo()).first;
  it->second.file_handler_extensions = std::move(file_handler_extensions);
  it->second.file_handler_mime_types = std::move(file_handler_mime_types);
  if (it->second.IsEmpty())
    handlers.erase(it);
  SetAppInfo(app_id, /*installed_profiles=*/nullptr,
             /*last_active_profiles=*/nullptr, &handlers,
             /*cd_hash_hmac_base64=*/nullptr,
             /*notification_permission_status=*/nullptr);
}

void AppShimRegistry::SaveProtocolHandlersForAppAndProfile(
    const std::string& app_id,
    const base::FilePath& profile,
    std::set<std::string> protocol_handlers) {
  std::map<base::FilePath, HandlerInfo> handlers = GetHandlersForApp(app_id);
  auto it = handlers.emplace(profile, HandlerInfo()).first;
  it->second.protocol_handlers = std::move(protocol_handlers);
  if (it->second.IsEmpty())
    handlers.erase(it);
  SetAppInfo(app_id, /*installed_profiles=*/nullptr,
             /*last_active_profiles=*/nullptr, &handlers,
             /*cd_hash_hmac_base64=*/nullptr,
             /*notification_permission_status=*/nullptr);
}

std::map<base::FilePath, AppShimRegistry::HandlerInfo>
AppShimRegistry::GetHandlersForApp(const std::string& app_id) {
  const base::Value::Dict& cache = GetPrefService()->GetDict(kAppShims);
  const base::Value::Dict* app_info = cache.FindDict(app_id);
  if (!app_info)
    return {};
  const base::Value::Dict* handlers = app_info->FindDict(kHandlers);
  if (!handlers)
    return {};
  std::map<base::FilePath, HandlerInfo> result;
  for (auto profile_handler : *handlers) {
    const base::Value::Dict* dict = profile_handler.second.GetIfDict();
    if (!dict)
      continue;
    HandlerInfo info;
    info.file_handler_extensions =
        ValueListToSet(dict->FindList(kFileHandlerExtensions));
    info.file_handler_mime_types =
        ValueListToSet(dict->FindList(kFileHandlerMimeTypes));
    info.protocol_handlers = ValueListToSet(dict->FindList(kProtocolHandlers));
    result.emplace(GetFullProfilePath(profile_handler.first), std::move(info));
  }
  return result;
}

bool AppShimRegistry::HasSavedAnyCdHashes() const {
  return GetPrefService()->HasPrefPath(kAppShimsCdHashHmacKey);
}

std::optional<AppShimRegistry::HmacKey>
AppShimRegistry::GetExistingCdHashHmacKey(
    const os_crypt_async::Encryptor& encryptor) {
  std::string key_base64 = GetPrefService()->GetString(kAppShimsCdHashHmacKey);
  if (key_base64.empty()) {
    LogGetHmacKeyResult(GetHmacKeyResult::kNotFound);
    return std::nullopt;
  }

  // The key used for the HMACs of code directory hash values is encrypted then
  // base64-encoded before being stored in prefs. Do the inverse operations here
  // to load the key.
  std::string encrypted_key;
  if (!base::Base64Decode(key_base64, &encrypted_key)) {
    LogGetHmacKeyResult(GetHmacKeyResult::kBase64DecodeFailed);
    return std::nullopt;
  }

  os_crypt_async::Encryptor::DecryptFlags flags;
  std::string key;
  if (!encryptor.DecryptString(encrypted_key, &key, &flags)) {
    if (flags.temporarily_unavailable) {
      LogGetHmacKeyResult(GetHmacKeyResult::kDecryptFailed_Temporary);
    } else {
      LogGetHmacKeyResult(GetHmacKeyResult::kDecryptFailed_Permanent);
    }
    return std::nullopt;
  }

  if (key.length() != kHmacKeySize) {
    LogGetHmacKeyResult(GetHmacKeyResult::kInvalidLength);
    return std::nullopt;
  }
  LogGetHmacKeyResult(GetHmacKeyResult::kSuccess);
  return std::make_optional<HmacKey>(key.begin(), key.end());
}

// Encrypt the key using OSCrypt and base64-encode the encrypted data before
// storing it in prefs.
bool AppShimRegistry::SaveCdHashHmacKey(
    const os_crypt_async::Encryptor& encryptor,
    const HmacKey& key) {
  std::string key_str(key.begin(), key.end());
  std::optional<std::vector<uint8_t>> encrypted_key =
      encryptor.EncryptString(key_str);
  if (!encrypted_key.has_value()) {
    LogSaveHmacKeyResult(SaveHmacKeyResult::kEncryptionFailed);
    return false;
  }

  GetPrefService()->SetString(kAppShimsCdHashHmacKey,
                              base::Base64Encode(*encrypted_key));
  LogSaveHmacKeyResult(SaveHmacKeyResult::kSuccess);
  return true;
}

AppShimRegistry::HmacKey AppShimRegistry::GetCdHashHmacKey(
    const os_crypt_async::Encryptor& encryptor) {
  if (hmac_key_.has_value()) {
    // If the key has not successfully been saved to prefs yet, retry encrypting
    // and storing to prefs, as the keychain might have become available.
    if (!hmac_key_saved_to_prefs_) {
      hmac_key_saved_to_prefs_ = SaveCdHashHmacKey(encryptor, *hmac_key_);
    }
    return *hmac_key_;
  }

  // If there is no cached key, try to load one from prefs.
  if (auto key = GetExistingCdHashHmacKey(encryptor); key.has_value()) {
    hmac_key_ = std::move(key);
    hmac_key_saved_to_prefs_ = true;
    return *hmac_key_;
  }

  // Either no key was stored in prefs, or the key that was stored could not be
  // decoded or decrypted. Generate and store a new random key. This will
  // invalidate any HMACs that were created with a previous key. The caller is
  // expected to handle this by re-creating the affected app shims and storing
  // the new code directory hash.
  hmac_key_.emplace(kHmacKeySize);
  crypto::RandBytes(*hmac_key_);
  hmac_key_saved_to_prefs_ = SaveCdHashHmacKey(encryptor, *hmac_key_);
  return *hmac_key_;
}

void AppShimRegistry::SaveCdHashForApp(const std::string& app_id,
                                       base::span<const uint8_t> cd_hash,
                                       base::OnceClosure callback) {
  // base::Unretained is safe, since AppShimRegistry is a singleton that is
  // never destructed.
  g_browser_process->os_crypt_async()->GetInstance(
      base::BindOnce(&AppShimRegistry::DoSaveCdHashForApp,
                     base::Unretained(this), app_id,
                     std::vector<uint8_t>(cd_hash.begin(), cd_hash.end()))
          .Then(std::move(callback)));
}

void AppShimRegistry::DoSaveCdHashForApp(const std::string& app_id,
                                         std::vector<uint8_t> cd_hash,
                                         os_crypt_async::Encryptor encryptor) {
  HmacKey hmac_key = GetCdHashHmacKey(encryptor);
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(hmac_key));

  std::array<uint8_t, 32> cd_hash_hmac;
  CHECK(hmac.Sign(cd_hash, cd_hash_hmac));

  std::string cd_hash_hmac_base64 = base::Base64Encode(cd_hash_hmac);
  SetAppInfo(app_id, /*installed_profiles=*/nullptr,
             /*last_active_profiles=*/nullptr, /*handlers=*/nullptr,
             &cd_hash_hmac_base64,
             /*notification_permission_status=*/nullptr);
}

void AppShimRegistry::VerifyCdHashForApp(
    const std::string& app_id,
    base::span<const uint8_t> cd_hash,
    base::OnceCallback<void(bool)> callback) {
  // base::Unretained is safe, since AppShimRegistry is a singleton that is
  // never destructed.
  g_browser_process->os_crypt_async()->GetInstance(
      base::BindOnce(&AppShimRegistry::DoVerifyCdHashForApp,
                     base::Unretained(this), app_id,
                     std::vector<uint8_t>(cd_hash.begin(), cd_hash.end()))
          .Then(std::move(callback)));
}

bool AppShimRegistry::DoVerifyCdHashForApp(
    const std::string& app_id,
    std::vector<uint8_t> cd_hash,
    os_crypt_async::Encryptor encryptor) {
  const base::Value::Dict& cache = GetPrefService()->GetDict(kAppShims);
  const base::Value::Dict* app_info = cache.FindDict(app_id);
  if (!app_info) {
    LOG(WARNING) << "No info found for app_id";
    return false;
  }

  const std::string* cd_hash_hmac_base64 = app_info->FindString(kCdHashHmac);
  if (!cd_hash_hmac_base64 || cd_hash_hmac_base64->empty()) {
    LOG(WARNING) << "App shim has no associated code directory hash";
    return false;
  }

  auto cd_hash_hmac = base::Base64Decode(*cd_hash_hmac_base64);
  if (!cd_hash_hmac) {
    LOG(WARNING) << "App shim's code directory hash could not be decoded";
    return false;
  }

  HmacKey hmac_key = GetCdHashHmacKey(encryptor);
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(hmac_key));
  return hmac.Verify(cd_hash, *cd_hash_hmac);
}

void AppShimRegistry::SaveNotificationPermissionStatusForApp(
    const std::string& app_id,
    mac_notifications::mojom::PermissionStatus status) {
  SetAppInfo(app_id, /*installed_profiles=*/nullptr,
             /*last_active_profiles=*/nullptr, /*handlers=*/nullptr,
             /*cd_hash_hmac_base64=*/nullptr, &status);
}

mac_notifications::mojom::PermissionStatus
AppShimRegistry::GetNotificationPermissionStatusForApp(
    const std::string& app_id) {
  using PermissionStatus = mac_notifications::mojom::PermissionStatus;
  const base::Value::Dict& cache = GetPrefService()->GetDict(kAppShims);
  const base::Value::Dict* app_info = cache.FindDict(app_id);
  if (!app_info) {
    return PermissionStatus::kNotDetermined;
  }
  std::optional<int> status_as_int =
      app_info->FindInt(kNotificationPermissionStatus);
  if (!status_as_int.has_value()) {
    return PermissionStatus::kNotDetermined;
  }
  switch (*status_as_int) {
    case static_cast<int>(PermissionStatus::kNotDetermined):
    case static_cast<int>(PermissionStatus::kPromptPending):
    case static_cast<int>(PermissionStatus::kDenied):
    case static_cast<int>(PermissionStatus::kGranted):
      return static_cast<PermissionStatus>(*status_as_int);
  }
  return PermissionStatus::kNotDetermined;
}

base::CallbackListSubscription AppShimRegistry::RegisterAppChangedCallback(
    base::RepeatingCallback<void(const std::string&)> callback) {
  return app_changed_callbacks_.Add(std::move(callback));
}

void AppShimRegistry::SetPrefServiceAndUserDataDirForTesting(
    PrefService* pref_service,
    const base::FilePath& user_data_dir) {
  override_pref_service_ = pref_service;
  override_user_data_dir_ = user_data_dir;
}

base::Value::Dict AppShimRegistry::AsDebugDict() const {
  const base::Value::Dict& app_shims = GetPrefService()->GetDict(kAppShims);

  return app_shims.Clone();
}

AppShimRegistry::AppShimRegistry() = default;
AppShimRegistry::~AppShimRegistry() = default;

PrefService* AppShimRegistry::GetPrefService() const {
  if (override_pref_service_)
    return override_pref_service_;
  return g_browser_process->local_state();
}

base::FilePath AppShimRegistry::GetFullProfilePath(
    const std::string& profile_path) const {
  base::FilePath relative_profile_path(profile_path);
  if (!override_user_data_dir_.empty())
    return override_user_data_dir_.Append(relative_profile_path);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->user_data_dir().Append(relative_profile_path);
}

void AppShimRegistry::SetAppInfo(
    const std::string& app_id,
    const std::set<base::FilePath>* installed_profiles,
    const std::set<base::FilePath>* last_active_profiles,
    const std::map<base::FilePath, HandlerInfo>* handlers,
    const std::string* cd_hash_hmac_base64,
    const mac_notifications::mojom::PermissionStatus*
        notification_permission_status) {
  ScopedDictPrefUpdate update(GetPrefService(), kAppShims);

  // If there are no installed profiles, clear the app's key.
  if (installed_profiles && installed_profiles->empty()) {
    update->Remove(app_id);
    return;
  }

  // Look up dictionary for the app.
  base::Value::Dict* app_info = update->FindDict(app_id);
  if (!app_info) {
    // If the key for the app doesn't exist, don't add it unless we are
    // specifying a new |installed_profiles| (e.g, for when the app exits
    // during uninstall and tells us its last-used profile after we just
    // removed the entry for the app).
    if (!installed_profiles)
      return;
    app_info = update->EnsureDict(app_id);
  }
  if (installed_profiles) {
    base::Value::List values;
    for (const auto& profile : *installed_profiles)
      values.Append(profile.BaseName().value());
    app_info->Set(kInstalledProfiles, std::move(values));
  }
  if (last_active_profiles) {
    base::Value::List values;
    for (const auto& profile : *last_active_profiles)
      values.Append(profile.BaseName().value());
    app_info->Set(kLastActiveProfiles, std::move(values));
  }
  if (handlers) {
    base::Value::Dict values;
    for (const auto& profile_handlers : *handlers) {
      base::Value::Dict value;
      value.Set(
          kFileHandlerExtensions,
          SetToValueList(profile_handlers.second.file_handler_extensions));
      value.Set(
          kFileHandlerMimeTypes,
          SetToValueList(profile_handlers.second.file_handler_mime_types));
      value.Set(kProtocolHandlers,
                SetToValueList(profile_handlers.second.protocol_handlers));
      values.Set(profile_handlers.first.BaseName().value(), std::move(value));
    }
    app_info->Set(kHandlers, std::move(values));
  }
  if (cd_hash_hmac_base64) {
    app_info->Set(kCdHashHmac, *cd_hash_hmac_base64);
  }
  if (notification_permission_status) {
    app_info->Set(kNotificationPermissionStatus,
                  static_cast<int>(*notification_permission_status));
  }
  app_changed_callbacks_.Notify(app_id);
}
