// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

SavedTabGroupServiceFactory& SavedTabGroupServiceFactory::GetInstance() {
  static base::NoDestructor<SavedTabGroupServiceFactory> instance;
  return *instance;
}

// static
SavedTabGroupKeyedService* SavedTabGroupServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SavedTabGroupKeyedService*>(
      GetInstance().GetServiceForBrowserContext(profile, true /* create */));
}

SavedTabGroupServiceFactory::SavedTabGroupServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SavedTabGroupKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

SavedTabGroupServiceFactory::~SavedTabGroupServiceFactory() = default;

KeyedService* SavedTabGroupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SavedTabGroupKeyedService(profile);
}
