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
  static base::NoDestructor<FindBarStateFactory> instance;
  return instance.get();
}

FindBarStateFactory::FindBarStateFactory()
    : ProfileKeyedServiceFactory(
          "FindBarState",
          // Separate instance in incognito.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

FindBarStateFactory::~FindBarStateFactory() = default;

std::unique_ptr<KeyedService>
FindBarStateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FindBarState>(context);
}
