// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

class Profile;

namespace tabs {

// Handles the glic actor task icon window controllers in chrome, only regular,
// non-OTR profiles are supported.
class GlicActorTaskIconManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static GlicActorTaskIconManagerFactory* GetInstance();
  static GlicActorTaskIconManager* GetForProfile(Profile* profile);

  GlicActorTaskIconManagerFactory(const GlicActorTaskIconManagerFactory&) =
      delete;
  GlicActorTaskIconManagerFactory& operator=(
      const GlicActorTaskIconManagerFactory&) = delete;

 private:
  friend base::NoDestructor<GlicActorTaskIconManagerFactory>;
  GlicActorTaskIconManagerFactory();
  ~GlicActorTaskIconManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_FACTORY_H_
