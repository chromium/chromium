// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

class Profile;

class SavedTabGroupServiceFactory : public ProfileKeyedServiceFactory {
 public:
  SavedTabGroupServiceFactory();
  SavedTabGroupServiceFactory(const SavedTabGroupServiceFactory&) = delete;
  void operator=(const SavedTabGroupServiceFactory&) = delete;
  ~SavedTabGroupServiceFactory() override;

  static SavedTabGroupServiceFactory* GetInstance();
  static SavedTabGroupKeyedService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SavedTabGroupServiceFactory>;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SERVICE_FACTORY_H_
