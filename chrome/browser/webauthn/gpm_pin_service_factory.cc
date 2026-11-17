// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_pin_service_factory.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/gpm_pin_service.h"
#include "device/fido/public/features.h"

namespace webauthn {

// static
GpmPinService* GpmPinServiceFactory::GetForProfile(Profile* profile) {
  if (!base::FeatureList::IsEnabled(device::kWebAuthnLocalGpmPasskeys)) {
    return nullptr;
  }
  return static_cast<GpmPinService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
GpmPinServiceFactory* GpmPinServiceFactory::GetInstance() {
  static base::NoDestructor<GpmPinServiceFactory> instance;
  return instance.get();
}

GpmPinServiceFactory::GpmPinServiceFactory()
    : ProfileKeyedServiceFactory("GpmPinService",
                                 ProfileSelections::BuildForRegularProfile()) {
  CHECK(base::FeatureList::IsEnabled(device::kWebAuthnLocalGpmPasskeys));
}

GpmPinServiceFactory::~GpmPinServiceFactory() = default;

std::unique_ptr<KeyedService>
GpmPinServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(base::FeatureList::IsEnabled(device::kWebAuthnLocalGpmPasskeys));
  return std::make_unique<GpmPinService>();
}

}  // namespace webauthn
