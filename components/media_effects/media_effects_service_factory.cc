// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/media_effects_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

// static
MediaEffectsService* MediaEffectsServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<MediaEffectsService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
MediaEffectsServiceFactory* MediaEffectsServiceFactory::GetInstance() {
  static base::NoDestructor<MediaEffectsServiceFactory> instance;
  return instance.get();
}

MediaEffectsServiceFactory::MediaEffectsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaEffectsServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

MediaEffectsServiceFactory::~MediaEffectsServiceFactory() = default;

std::unique_ptr<KeyedService>
MediaEffectsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  CHECK(browser_context);
  return std::make_unique<MediaEffectsService>(
      user_prefs::UserPrefs::Get(browser_context));
}
