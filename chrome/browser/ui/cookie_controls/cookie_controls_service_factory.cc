// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
CookieControlsService* CookieControlsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CookieControlsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CookieControlsServiceFactory* CookieControlsServiceFactory::GetInstance() {
  return base::Singleton<CookieControlsServiceFactory>::get();
}

// static
KeyedService* CookieControlsServiceFactory::BuildInstanceFor(Profile* profile) {
  return new CookieControlsService(profile);
}

CookieControlsServiceFactory::CookieControlsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CookieControlsService",
          BrowserContextDependencyManager::GetInstance()) {}

CookieControlsServiceFactory::~CookieControlsServiceFactory() = default;

content::BrowserContext* CookieControlsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The incognito profile has its own CookieSettings. Therefore, it should
  // get its own CookieControlsService.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* CookieControlsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(Profile::FromBrowserContext(profile));
}
