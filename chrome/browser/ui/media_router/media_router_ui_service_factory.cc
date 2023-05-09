// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"

using content::BrowserContext;

namespace media_router {

// static
MediaRouterUIService* MediaRouterUIServiceFactory::GetForBrowserContext(
    BrowserContext* context) {
  DCHECK(context);
  return static_cast<MediaRouterUIService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
MediaRouterUIServiceFactory* MediaRouterUIServiceFactory::GetInstance() {
  return base::Singleton<MediaRouterUIServiceFactory>::get();
}

MediaRouterUIServiceFactory::MediaRouterUIServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaRouterUIService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ChromeMediaRouterFactory::GetInstance());
  // MediaRouterUIService owns a MediaRouterActionController that depends on
  // ToolbarActionsModel.
  DependsOn(ToolbarActionsModelFactory::GetInstance());
}

MediaRouterUIServiceFactory::~MediaRouterUIServiceFactory() {}

KeyedService* MediaRouterUIServiceFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new MediaRouterUIService(Profile::FromBrowserContext(context));
}

#if !BUILDFLAG(IS_ANDROID)
bool MediaRouterUIServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
#endif

bool MediaRouterUIServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace media_router
