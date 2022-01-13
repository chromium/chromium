// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/cxx20_erase.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/ash/microphone_mute_notification_delegate_impl.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

apps::AppCapabilityAccessCache* GetAppCapabilityAccessCache(
    AccountId account_id) {
  return apps::AppCapabilityAccessCacheWrapper::Get()
      .GetAppCapabilityAccessCache(account_id);
}

apps::AppRegistryCache* GetActiveUserAppRegistryCache() {
  auto* manager = user_manager::UserManager::Get();
  const user_manager::User* active_user = manager->GetActiveUser();
  if (!active_user)
    return nullptr;

  auto account_id = active_user->GetAccountId();
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(active_user);
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  return &proxy->AppRegistryCache();
}

absl::optional<std::u16string> MapAppIdToShortName(
    std::string app_id,
    apps::AppCapabilityAccessCache* capability_cache,
    apps::AppRegistryCache* registry_cache) {
  DCHECK(capability_cache);
  DCHECK(registry_cache);

  for (const std::string& app :
       capability_cache->GetAppsAccessingMicrophone()) {
    absl::optional<std::u16string> name;
    registry_cache->ForOneApp(app,
                              [&app_id, &name](const apps::AppUpdate& update) {
                                if (update.AppId() == app_id)
                                  name = base::UTF8ToUTF16(update.ShortName());
                              });
    if (name.has_value())
      return name;
  }

  return absl::nullopt;
}

}  // namespace

MicrophoneMuteNotificationDelegateImpl::
    MicrophoneMuteNotificationDelegateImpl() {
  // These checks are needed for testing, where SessionManager and/or
  // UserManager may not exist.
  session_manager::SessionManager* sm = session_manager::SessionManager::Get();
  if (sm) {
    session_manager_observation_.Observe(sm);
  }
  user_manager::UserManager* um = user_manager::UserManager::Get();
  if (um) {
    user_session_state_observation_.Observe(um);
  }
}

MicrophoneMuteNotificationDelegateImpl::
    ~MicrophoneMuteNotificationDelegateImpl() = default;

absl::optional<std::u16string>
MicrophoneMuteNotificationDelegateImpl::GetAppAccessingMicrophone() {
  apps::AppRegistryCache* reg_cache = GetActiveUserAppRegistryCache();
  apps::AppCapabilityAccessCache* cap_cache =
      GetActiveUserAppCapabilityAccessCache();
  // A reg_cache and/or cap_cache of value nullptr is possible if we have
  // no active user, e.g. the login screen, so we test and return nullopt
  // in that case instead of using DCHECK().
  if (!reg_cache || !cap_cache)
    return absl::nullopt;
  return GetAppAccessingMicrophone(cap_cache, reg_cache);
}

void MicrophoneMuteNotificationDelegateImpl::OnCapabilityAccessUpdate(
    const apps::CapabilityAccessUpdate& update) {
  base::Erase(mic_using_app_ids[active_user_account_id_], update.AppId());

  if (update.Microphone() == apps::mojom::OptionalBool::kTrue) {
    mic_using_app_ids[active_user_account_id_].push_front(update.AppId());
  }
}

void MicrophoneMuteNotificationDelegateImpl::
    OnAppCapabilityAccessCacheWillBeDestroyed(
        apps::AppCapabilityAccessCache* cache) {
  app_capability_access_cache_observation_.Reset();
}

//
// A couple of notes on why we have OnSessionStateChanged() and
// ActiveUserChanged(), i.e. why we observe both SessionManager and UserManager.
//
// The critical logic here is based on knowing when an app starts or stops
// attempting to use the microphone, and for this we observe the active user's
// AppCapabilityAccessCache.  When the active user's AppCapabilityAccessCache
// changes, we need to stop observing any AppCapabilityAccessCache we were
// previously observing and start observing the currently active one.  This is
// the job of CheckActiveUserChanged().
//

void MicrophoneMuteNotificationDelegateImpl::OnSessionStateChanged() {
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  if (state == session_manager::SessionState::ACTIVE) {
    CheckActiveUserChanged();
    session_manager_observation_.Reset();
  }
}

void MicrophoneMuteNotificationDelegateImpl::ActiveUserChanged(
    user_manager::User* active_user) {
  CheckActiveUserChanged();
}

AccountId MicrophoneMuteNotificationDelegateImpl::GetActiveUserAccountId() {
  auto* manager = user_manager::UserManager::Get();
  const user_manager::User* active_user = manager->GetActiveUser();
  if (!active_user)
    return EmptyAccountId();

  return active_user->GetAccountId();
}

void MicrophoneMuteNotificationDelegateImpl::CheckActiveUserChanged() {
  AccountId id = GetActiveUserAccountId();
  if (id == EmptyAccountId() || id == active_user_account_id_)
    return;

  if (active_user_account_id_ != EmptyAccountId()) {
    app_capability_access_cache_observation_.Reset();
    active_user_account_id_ = EmptyAccountId();
  }

  apps::AppCapabilityAccessCache* cap_cache = GetAppCapabilityAccessCache(id);
  if (cap_cache) {
    app_capability_access_cache_observation_.Observe(cap_cache);
    active_user_account_id_ = id;
  }
}

apps::AppCapabilityAccessCache* MicrophoneMuteNotificationDelegateImpl::
    GetActiveUserAppCapabilityAccessCache() {
  return GetAppCapabilityAccessCache(GetActiveUserAccountId());
}

absl::optional<std::u16string>
MicrophoneMuteNotificationDelegateImpl::GetAppAccessingMicrophone(
    apps::AppCapabilityAccessCache* capability_cache,
    apps::AppRegistryCache* registry_cache) {
  if (mic_using_app_ids[active_user_account_id_].empty())
    return absl::nullopt;

  return MapAppIdToShortName(mic_using_app_ids[active_user_account_id_].front(),
                             capability_cache, registry_cache);
}
