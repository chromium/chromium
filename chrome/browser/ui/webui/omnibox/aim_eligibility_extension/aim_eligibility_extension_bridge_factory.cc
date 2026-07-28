// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge.h"
#include "components/omnibox/common/omnibox_features.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"

// static
AimEligibilityExtensionBridge*
AimEligibilityExtensionBridgeFactory::GetForProfile(Profile* profile) {
  return static_cast<AimEligibilityExtensionBridge*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AimEligibilityExtensionBridgeFactory*
AimEligibilityExtensionBridgeFactory::GetInstance() {
  static base::NoDestructor<AimEligibilityExtensionBridgeFactory> instance;
  return instance.get();
}

AimEligibilityExtensionBridgeFactory::AimEligibilityExtensionBridgeFactory()
    : ProfileKeyedServiceFactory(
          "AimEligibilityExtensionBridge",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(AimEligibilityServiceFactory::GetInstance());
  DependsOn(extensions::ExtensionMojoBinderRegistryFactory::GetInstance());
}

AimEligibilityExtensionBridgeFactory::~AimEligibilityExtensionBridgeFactory() =
    default;

std::unique_ptr<KeyedService>
AimEligibilityExtensionBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityComponentExtension)) {
    return nullptr;
  }
  return std::make_unique<AimEligibilityExtensionBridge>(
      Profile::FromBrowserContext(context));
}

bool AimEligibilityExtensionBridgeFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(
      omnibox::kAimEligibilityComponentExtension);
}
