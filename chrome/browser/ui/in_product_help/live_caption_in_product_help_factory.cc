// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/live_caption_in_product_help_factory.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/in_product_help/live_caption_in_product_help.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
LiveCaptionInProductHelp* LiveCaptionInProductHelpFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LiveCaptionInProductHelp*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LiveCaptionInProductHelpFactory*
LiveCaptionInProductHelpFactory::GetInstance() {
  static base::NoDestructor<LiveCaptionInProductHelpFactory> factory;
  return factory.get();
}

LiveCaptionInProductHelpFactory::LiveCaptionInProductHelpFactory()
    : BrowserContextKeyedServiceFactory(
          "LiveCaptionInProductHelp",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
}

LiveCaptionInProductHelpFactory::~LiveCaptionInProductHelpFactory() = default;

content::BrowserContext*
LiveCaptionInProductHelpFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* LiveCaptionInProductHelpFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LiveCaptionInProductHelp(Profile::FromBrowserContext(context));
}
