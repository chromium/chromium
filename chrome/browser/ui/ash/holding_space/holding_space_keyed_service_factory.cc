// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"

#include "ash/public/cpp/ash_features.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/chromeos/fileapi/file_change_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace ash {

// static
HoldingSpaceKeyedServiceFactory*
HoldingSpaceKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<HoldingSpaceKeyedServiceFactory> factory;
  return factory.get();
}

HoldingSpaceKeyedServiceFactory::HoldingSpaceKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "HoldingSpaceService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::FileChangeServiceFactory::GetInstance());
  DependsOn(file_manager::VolumeManagerFactory::GetInstance());
}

HoldingSpaceKeyedService* HoldingSpaceKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<HoldingSpaceKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

content::BrowserContext*
HoldingSpaceKeyedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  // Guest users are supported but should redirect to create the holding space
  // service for the original (e.g. non-incognito) profile.
  if (user->GetType() == user_manager::USER_TYPE_GUEST)
    return profile->GetOriginalProfile();

  // Don't create the service for off the record profiles of other user types.
  return profile->IsOffTheRecord() ? nullptr : context;
}

KeyedService* HoldingSpaceKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!features::IsTemporaryHoldingSpaceEnabled())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord());

  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  if (user->GetType() == user_manager::USER_TYPE_KIOSK_APP)
    return nullptr;

  return new HoldingSpaceKeyedService(profile, user->GetAccountId());
}

void HoldingSpaceKeyedServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  HoldingSpaceKeyedService::RegisterProfilePrefs(registry);
}

}  // namespace ash
