// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"

#include "chrome/browser/ui/find_bar/find_bar_state.h"

// static
FindBarState* FindBarStateFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FindBarState*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
FindBarStateFactory* FindBarStateFactory::GetInstance() {
  return base::Singleton<FindBarStateFactory>::get();
}

FindBarStateFactory::FindBarStateFactory()
    : ProfileKeyedServiceFactory(
          "FindBarState",
          // Separate instance in incognito.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

FindBarStateFactory::~FindBarStateFactory() = default;

KeyedService* FindBarStateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FindBarState(context);
}
