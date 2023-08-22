// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_FACTORY_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

class ToolbarActionsModel;

class ToolbarActionsModelFactory : public ProfileKeyedServiceFactory {
 public:
  static ToolbarActionsModel* GetForProfile(Profile* profile);

  static ToolbarActionsModelFactory* GetInstance();

 private:
  friend base::NoDestructor<ToolbarActionsModelFactory>;

  ToolbarActionsModelFactory();
  ~ToolbarActionsModelFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_FACTORY_H_
