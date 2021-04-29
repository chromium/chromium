// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/feature_tutorial_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/feature_tutorial_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

FeatureTutorialServiceFactory::FeatureTutorialServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FeatureTutorialService",
          BrowserContextDependencyManager::GetInstance()) {}

// static
FeatureTutorialServiceFactory* FeatureTutorialServiceFactory::GetInstance() {
  static base::NoDestructor<FeatureTutorialServiceFactory> instance;
  return instance.get();
}

// static
FeatureTutorialService* FeatureTutorialServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<FeatureTutorialService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* FeatureTutorialServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return FeatureTutorialService::MakeInstance(profile).release();
}
