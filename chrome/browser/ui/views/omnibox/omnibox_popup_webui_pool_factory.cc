// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"

// static
OmniboxPopupWebUIPoolFactory* OmniboxPopupWebUIPoolFactory::GetInstance() {
  static base::NoDestructor<OmniboxPopupWebUIPoolFactory> instance;
  return instance.get();
}

// static
OmniboxPopupWebUIPool* OmniboxPopupWebUIPoolFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OmniboxPopupWebUIPool*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

OmniboxPopupWebUIPoolFactory::OmniboxPopupWebUIPoolFactory()
    : ProfileKeyedServiceFactory(
          "OmniboxPopupWebUIPool",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

OmniboxPopupWebUIPoolFactory::~OmniboxPopupWebUIPoolFactory() = default;

std::unique_ptr<KeyedService>
OmniboxPopupWebUIPoolFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<OmniboxPopupWebUIPool>();
}
