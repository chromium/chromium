// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/permissions_manager.h"

// static
ToolbarActionsModel* ToolbarActionsModelFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ToolbarActionsModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ToolbarActionsModelFactory* ToolbarActionsModelFactory::GetInstance() {
  static base::NoDestructor<ToolbarActionsModelFactory> instance;
  return instance.get();
}

ToolbarActionsModelFactory::ToolbarActionsModelFactory()
    : ProfileKeyedServiceFactory(
          "ToolbarActionsModel",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(extensions::ExtensionActionAPI::GetFactoryInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(extensions::ExtensionManagementFactory::GetInstance());
  DependsOn(extensions::PermissionsManager::GetFactory());
}

ToolbarActionsModelFactory::~ToolbarActionsModelFactory() = default;

std::unique_ptr<KeyedService>
ToolbarActionsModelFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ToolbarActionsModel>(
      Profile::FromBrowserContext(context),
      extensions::ExtensionPrefsFactory::GetForBrowserContext(context));
}

bool ToolbarActionsModelFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ToolbarActionsModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
