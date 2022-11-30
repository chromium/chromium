// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/calendar/calendar_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service.h"
#include "components/user_manager/user.h"

namespace ash {

// static
CalendarKeyedServiceFactory* CalendarKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<CalendarKeyedServiceFactory> factory;
  return factory.get();
}

CalendarKeyedServiceFactory::CalendarKeyedServiceFactory()
    : ProfileKeyedServiceFactory("CalendarKeyedService") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

CalendarKeyedService* CalendarKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<CalendarKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

KeyedService* CalendarKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  if (!user->HasGaiaAccount())
    return nullptr;

  return new CalendarKeyedService(profile, user->GetAccountId());
}

}  // namespace ash
