// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promo_trigger_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"
#include "components/sharing_message/features.h"

// static
IOSPromoTriggerService* IOSPromoTriggerServiceFactory::GetForProfile(
    Profile* profile) {
  if (MobilePromoOnDesktopTypeEnabled() ==
      MobilePromoOnDesktopPromoType::kDisabled) {
    return nullptr;
  }
  return static_cast<IOSPromoTriggerService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
IOSPromoTriggerServiceFactory* IOSPromoTriggerServiceFactory::GetInstance() {
  static base::NoDestructor<IOSPromoTriggerServiceFactory> instance;
  return instance.get();
}

IOSPromoTriggerServiceFactory::IOSPromoTriggerServiceFactory()
    : ProfileKeyedServiceFactory(
          "IOSPromoTriggerService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {}

IOSPromoTriggerServiceFactory::~IOSPromoTriggerServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSPromoTriggerServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<IOSPromoTriggerService>();
}
