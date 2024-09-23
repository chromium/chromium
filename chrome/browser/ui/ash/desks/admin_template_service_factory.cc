// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/admin_template_service_factory.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"

namespace ash {

// static
desks_storage::AdminTemplateService* AdminTemplateServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<desks_storage::AdminTemplateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AdminTemplateServiceFactory* AdminTemplateServiceFactory::GetInstance() {
  static base::NoDestructor<AdminTemplateServiceFactory> instance;
  return instance.get();
}

AdminTemplateServiceFactory::AdminTemplateServiceFactory()
    : ProfileKeyedServiceFactory(
          "AdminTemplateService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

KeyedService* AdminTemplateServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return new desks_storage::AdminTemplateService(
      profile->GetPath(), multi_user_util::GetAccountIdFromProfile(profile),
      profile->GetPrefs());
}

}  // namespace ash
