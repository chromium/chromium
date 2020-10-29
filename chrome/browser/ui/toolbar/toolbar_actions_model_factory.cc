// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"

// static
ToolbarActionsModel* ToolbarActionsModelFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ToolbarActionsModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ToolbarActionsModelFactory* ToolbarActionsModelFactory::GetInstance() {
  return base::Singleton<ToolbarActionsModelFactory>::get();
}

ToolbarActionsModelFactory::ToolbarActionsModelFactory()
    : BrowserContextKeyedServiceFactory(
          "ToolbarActionsModel",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionActionAPI::GetFactoryInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(extensions::ExtensionManagementFactory::GetInstance());
}

ToolbarActionsModelFactory::~ToolbarActionsModelFactory() {}

KeyedService* ToolbarActionsModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ToolbarActionsModel(
      Profile::FromBrowserContext(context),
      extensions::ExtensionPrefsFactory::GetForBrowserContext(context));
}

content::BrowserContext* ToolbarActionsModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

bool ToolbarActionsModelFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ToolbarActionsModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
