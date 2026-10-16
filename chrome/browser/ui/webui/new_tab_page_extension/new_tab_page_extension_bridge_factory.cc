// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_bridge_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_bridge.h"
#include "components/search/ntp_features.h"
#include "extensions/browser/extension_config_map_factory.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"

// static
NewTabPageExtensionBridge* NewTabPageExtensionBridgeFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NewTabPageExtensionBridge*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NewTabPageExtensionBridgeFactory*
NewTabPageExtensionBridgeFactory::GetInstance() {
  static base::NoDestructor<NewTabPageExtensionBridgeFactory> instance;
  return instance.get();
}

NewTabPageExtensionBridgeFactory::NewTabPageExtensionBridgeFactory()
    : ProfileKeyedServiceFactory(
          "NewTabPageExtensionBridge",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(extensions::ExtensionConfigMapFactory::GetInstance());
  DependsOn(extensions::ExtensionMojoBinderRegistryFactory::GetInstance());
  // Used by SanitizedImageSource, which the bridge registers.
  DependsOn(IdentityManagerFactory::GetInstance());
}

NewTabPageExtensionBridgeFactory::~NewTabPageExtensionBridgeFactory() = default;

std::unique_ptr<KeyedService>
NewTabPageExtensionBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpComponentExtension)) {
    return nullptr;
  }
  return std::make_unique<NewTabPageExtensionBridge>(
      Profile::FromBrowserContext(context));
}

bool NewTabPageExtensionBridgeFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(ntp_features::kNtpComponentExtension);
}
