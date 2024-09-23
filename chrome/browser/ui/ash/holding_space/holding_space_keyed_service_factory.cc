// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/fileapi/file_change_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace ash {
namespace {

HoldingSpaceKeyedServiceFactory::GlobalTestingFactory* GetTestingFactory() {
  static base::NoDestructor<
      HoldingSpaceKeyedServiceFactory::GlobalTestingFactory>
      testing_factory_;
  return testing_factory_.get();
}

}  // namespace

// static
HoldingSpaceKeyedServiceFactory*
HoldingSpaceKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<HoldingSpaceKeyedServiceFactory> factory;
  return factory.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
HoldingSpaceKeyedServiceFactory::GetDefaultTestingFactory() {
  return base::BindRepeating([](content::BrowserContext* context) {
    return BuildServiceInstanceForInternal(context);
  });
}

// static
void HoldingSpaceKeyedServiceFactory::SetTestingFactory(
    GlobalTestingFactory testing_factory) {
  *GetTestingFactory() = std::move(testing_factory);
}

HoldingSpaceKeyedServiceFactory::HoldingSpaceKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "HoldingSpaceService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(arc::ArcFileSystemBridge::GetFactory());
  DependsOn(FileChangeServiceFactory::GetInstance());
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
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
  Profile* const profile = Profile::FromBrowserContext(context);

  // Guest sessions are supported but redirect to the primary OTR profile.
  if (profile->IsGuestSession())
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // Don't create the service for OTR profiles outside of guest sessions.
  return profile->IsOffTheRecord() ? nullptr : context;
}

std::unique_ptr<KeyedService>
HoldingSpaceKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  GlobalTestingFactory* testing_factory = GetTestingFactory();
  return testing_factory->is_null() ? BuildServiceInstanceForInternal(context)
                                    : testing_factory->Run(context);
}

// static
std::unique_ptr<KeyedService>
HoldingSpaceKeyedServiceFactory::BuildServiceInstanceForInternal(
    content::BrowserContext* context) {
  Profile* const profile = Profile::FromBrowserContext(context);
  DCHECK_EQ(profile->IsGuestSession(), profile->IsOffTheRecord());

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  if (user->GetType() == user_manager::UserType::kKioskApp) {
    return nullptr;
  }

  return std::make_unique<HoldingSpaceKeyedService>(profile,
                                                    user->GetAccountId());
}

void HoldingSpaceKeyedServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  HoldingSpaceKeyedService::RegisterProfilePrefs(registry);
}

}  // namespace ash
