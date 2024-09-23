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
  static base::NoDestructor<MediaRouterUIServiceFactory> instance;
  return instance.get();
}

MediaRouterUIServiceFactory::MediaRouterUIServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaRouterUIService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ChromeMediaRouterFactory::GetInstance());
  // MediaRouterUIService owns a CastToolbarButtonController that depends on
  // ToolbarActionsModel.
  DependsOn(ToolbarActionsModelFactory::GetInstance());
}

MediaRouterUIServiceFactory::~MediaRouterUIServiceFactory() = default;

std::unique_ptr<KeyedService>
MediaRouterUIServiceFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<MediaRouterUIService>(
      Profile::FromBrowserContext(context));
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
