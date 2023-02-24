// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

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
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

SavedTabGroupServiceFactory::~SavedTabGroupServiceFactory() = default;

KeyedService* SavedTabGroupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(context);
  Profile* profile = Profile::FromBrowserContext(context);
  return new SavedTabGroupKeyedService(profile);
}
