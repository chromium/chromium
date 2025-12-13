// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"

#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace tabs {
using actor::ActorKeyedServiceFactory;

// static
GlicActorTaskIconManagerFactory*
GlicActorTaskIconManagerFactory::GetInstance() {
  static base::NoDestructor<GlicActorTaskIconManagerFactory> instance;
  return instance.get();
}

// static
GlicActorTaskIconManager* GlicActorTaskIconManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GlicActorTaskIconManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

GlicActorTaskIconManagerFactory::GlicActorTaskIconManagerFactory()
    : ProfileKeyedServiceFactory("GlicActorTaskIconManager") {
  DependsOn(ActorKeyedServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
GlicActorTaskIconManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GlicActorTaskIconManager>(
      profile, ActorKeyedServiceFactory::GetActorKeyedService(context));
}

}  // namespace tabs
