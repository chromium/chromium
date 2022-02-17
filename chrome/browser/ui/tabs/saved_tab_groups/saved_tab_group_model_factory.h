// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class SavedTabGroupModel;

class SavedTabGroupModelFactory : public BrowserContextKeyedServiceFactory {
 public:
  SavedTabGroupModelFactory();
  SavedTabGroupModelFactory(const SavedTabGroupModelFactory&) = delete;
  void operator=(const SavedTabGroupModelFactory&) = delete;
  ~SavedTabGroupModelFactory() override;

  static SavedTabGroupModelFactory& GetInstance();
  static SavedTabGroupModel* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SavedTabGroupModelFactory>;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_FACTORY_H_
