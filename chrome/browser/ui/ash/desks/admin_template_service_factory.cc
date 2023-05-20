// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/admin_template_service_factory.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"

namespace ash {

// static
desks_storage::AdminTemplateService* AdminTemplateServiceFactory::GetForProfile(
    Profile* profile) {
  // Service should not be available if the flag is not enabled.
  if (!base::FeatureList::IsEnabled(ash::features::kAppLaunchAutomation)) {
    LOG(WARNING) << "AppLaunchAutomation flag not set!";
    return nullptr;
  }

  return static_cast<desks_storage::AdminTemplateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AdminTemplateServiceFactory* AdminTemplateServiceFactory::GetInstance() {
  return base::Singleton<AdminTemplateServiceFactory>::get();
}

AdminTemplateServiceFactory::AdminTemplateServiceFactory()
    : ProfileKeyedServiceFactory(
          "AdminTemplateService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

KeyedService* AdminTemplateServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile);

  return new desks_storage::AdminTemplateService(profile->GetPath(),
                                                 account_id);
}

}  // namespace ash
