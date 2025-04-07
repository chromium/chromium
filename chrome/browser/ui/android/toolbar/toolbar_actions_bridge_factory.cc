// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/toolbar_actions_bridge_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/toolbar/toolbar_actions_bridge.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry_factory.h"

using extensions::ExtensionActionManager;
using extensions::ExtensionRegistryFactory;

// static
ToolbarActionsBridge* ToolbarActionsBridgeFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ToolbarActionsBridge*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ToolbarActionsBridgeFactory* ToolbarActionsBridgeFactory::GetInstance() {
  static base::NoDestructor<ToolbarActionsBridgeFactory> instance;
  return instance.get();
}

ToolbarActionsBridgeFactory::ToolbarActionsBridgeFactory()
    : ProfileKeyedServiceFactory(
          "ToolbarActionsBridge",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ExtensionActionManager::GetFactory());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ToolbarActionsModelFactory::GetInstance());
}

ToolbarActionsBridgeFactory::~ToolbarActionsBridgeFactory() = default;

std::unique_ptr<KeyedService>
ToolbarActionsBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ToolbarActionsBridge>(
      Profile::FromBrowserContext(context));
}

bool ToolbarActionsBridgeFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ToolbarActionsBridgeFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
