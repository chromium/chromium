// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/ash/microphone_mute_notification_delegate_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

MicrophoneMuteNotificationDelegateImpl::
    MicrophoneMuteNotificationDelegateImpl() = default;
MicrophoneMuteNotificationDelegateImpl::
    ~MicrophoneMuteNotificationDelegateImpl() = default;

absl::optional<std::u16string>
MicrophoneMuteNotificationDelegateImpl::GetAppAccessingMicrophone() {
  auto* manager = user_manager::UserManager::Get();
  const user_manager::User* active_user = manager->GetActiveUser();
  if (!active_user)
    return absl::nullopt;

  auto account_id = active_user->GetAccountId();
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(active_user);
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  apps::AppRegistryCache& reg_cache = proxy->AppRegistryCache();
  apps::AppCapabilityAccessCache* cap_cache =
      apps::AppCapabilityAccessCacheWrapper::Get().GetAppCapabilityAccessCache(
          account_id);
  DCHECK(cap_cache);
  return GetAppAccessingMicrophone(cap_cache, &reg_cache);
}

absl::optional<std::u16string>
MicrophoneMuteNotificationDelegateImpl::GetAppAccessingMicrophone(
    apps::AppCapabilityAccessCache* capability_cache,
    apps::AppRegistryCache* registry_cache) {
  bool found_app = false;
  DCHECK(capability_cache);
  DCHECK(registry_cache);

  for (const std::string& app :
       capability_cache->GetAppsAccessingMicrophone()) {
    found_app = true;
    std::u16string name;
    registry_cache->ForOneApp(app, [&name](const apps::AppUpdate& update) {
      name = base::UTF8ToUTF16(update.ShortName());
    });
    if (!name.empty()) {
      // We have an actual app name, so return it.
      return absl::optional<std::u16string>(name);
    }
  }

  // Returning an empty string means we found an app but no name, while
  // returning absl::nullopt means no app is using the mic.
  return found_app ? absl::make_optional(u"") : absl::nullopt;
}
