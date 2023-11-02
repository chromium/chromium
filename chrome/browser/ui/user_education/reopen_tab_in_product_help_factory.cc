// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/reopen_tab_in_product_help_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/reopen_tab_in_product_help.h"

ReopenTabInProductHelpFactory::ReopenTabInProductHelpFactory()
    : ProfileKeyedServiceFactory(
          "ReopenTabInProductHelp",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
}

ReopenTabInProductHelpFactory::~ReopenTabInProductHelpFactory() {}

// static
ReopenTabInProductHelpFactory* ReopenTabInProductHelpFactory::GetInstance() {
  return base::Singleton<ReopenTabInProductHelpFactory>::get();
}

// static
ReopenTabInProductHelp* ReopenTabInProductHelpFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ReopenTabInProductHelp*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* ReopenTabInProductHelpFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ReopenTabInProductHelp(Profile::FromBrowserContext(context),
                                    base::DefaultTickClock::GetInstance());
}
