// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

GlobalMediaControlsInProductHelpFactory::
    GlobalMediaControlsInProductHelpFactory()
    : BrowserContextKeyedServiceFactory(
          "GlobalMediaControlsInProductHelp",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
}

GlobalMediaControlsInProductHelpFactory::
    ~GlobalMediaControlsInProductHelpFactory() {}

// static
GlobalMediaControlsInProductHelpFactory*
GlobalMediaControlsInProductHelpFactory::GetInstance() {
  return base::Singleton<GlobalMediaControlsInProductHelpFactory>::get();
}

// static
GlobalMediaControlsInProductHelp*
GlobalMediaControlsInProductHelpFactory::GetForProfile(Profile* profile) {
  return static_cast<GlobalMediaControlsInProductHelp*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* GlobalMediaControlsInProductHelpFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new GlobalMediaControlsInProductHelp(
      Profile::FromBrowserContext(context));
}

content::BrowserContext*
GlobalMediaControlsInProductHelpFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
