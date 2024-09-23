// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"

// static
PinnedToolbarActionsModel* PinnedToolbarActionsModelFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PinnedToolbarActionsModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PinnedToolbarActionsModelFactory*
PinnedToolbarActionsModelFactory::GetInstance() {
  static base::NoDestructor<PinnedToolbarActionsModelFactory> instance;
  return instance.get();
}

PinnedToolbarActionsModelFactory::PinnedToolbarActionsModelFactory()
    : ProfileKeyedServiceFactory(
          "PinnedToolbarActionsModel",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

PinnedToolbarActionsModelFactory::~PinnedToolbarActionsModelFactory() = default;

std::unique_ptr<KeyedService>
PinnedToolbarActionsModelFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<PinnedToolbarActionsModel>(
      Profile::FromBrowserContext(context));
}

bool PinnedToolbarActionsModelFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool PinnedToolbarActionsModelFactory::ServiceIsNULLWhileTesting() const {
  return false;
}
