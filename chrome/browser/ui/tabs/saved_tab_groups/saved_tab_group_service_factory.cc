// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace tab_groups {

SavedTabGroupServiceFactory* SavedTabGroupServiceFactory::GetInstance() {
  static base::NoDestructor<SavedTabGroupServiceFactory> instance;
  return instance.get();
}

// static
SavedTabGroupKeyedService* SavedTabGroupServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  return static_cast<SavedTabGroupKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

SavedTabGroupServiceFactory::SavedTabGroupServiceFactory()
    : ProfileKeyedServiceFactory(
          "SavedTabGroupKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

SavedTabGroupServiceFactory::~SavedTabGroupServiceFactory() = default;

std::unique_ptr<KeyedService>
SavedTabGroupServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(!tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled());

  DCHECK(context);
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  return std::make_unique<SavedTabGroupKeyedService>(profile,
                                                     device_info_tracker);
}

}  // namespace tab_groups
