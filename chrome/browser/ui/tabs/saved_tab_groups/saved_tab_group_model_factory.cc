// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

SavedTabGroupModelFactory& SavedTabGroupModelFactory::GetInstance() {
  static base::NoDestructor<SavedTabGroupModelFactory> instance;
  return *instance;
}

// static
SavedTabGroupModel* SavedTabGroupModelFactory::GetForProfile(Profile* profile) {
  return static_cast<SavedTabGroupModel*>(
      GetInstance().GetServiceForBrowserContext(profile, true));
}

SavedTabGroupModelFactory::SavedTabGroupModelFactory()
    : BrowserContextKeyedServiceFactory(
          "SavedTabGroupModel",
          BrowserContextDependencyManager::GetInstance()) {}

SavedTabGroupModelFactory::~SavedTabGroupModelFactory() = default;

KeyedService* SavedTabGroupModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SavedTabGroupModel();
}
