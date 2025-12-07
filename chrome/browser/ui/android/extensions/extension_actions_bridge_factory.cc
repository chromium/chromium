// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_actions_bridge_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/extensions/extension_actions_bridge.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry_factory.h"

namespace extensions {

// static
ExtensionActionsBridge* ExtensionActionsBridgeFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionActionsBridge*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ExtensionActionsBridgeFactory* ExtensionActionsBridgeFactory::GetInstance() {
  static base::NoDestructor<ExtensionActionsBridgeFactory> instance;
  return instance.get();
}

ExtensionActionsBridgeFactory::ExtensionActionsBridgeFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionActionsBridge",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ExtensionActionManager::GetFactory());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ToolbarActionsModelFactory::GetInstance());
}

ExtensionActionsBridgeFactory::~ExtensionActionsBridgeFactory() = default;

std::unique_ptr<KeyedService>
ExtensionActionsBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionActionsBridge>(
      Profile::FromBrowserContext(context));
}

bool ExtensionActionsBridgeFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ExtensionActionsBridgeFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
