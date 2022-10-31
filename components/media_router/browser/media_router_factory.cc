// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_factory.h"

#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_router/browser/media_router.h"
#include "content/public/browser/browser_context.h"

using content::BrowserContext;

namespace media_router {

namespace {

MediaRouterFactory* g_instance = nullptr;

}  // namespace

// static
MediaRouter* MediaRouterFactory::GetApiForBrowserContext(
    BrowserContext* context) {
  DCHECK(context);
  // GetServiceForBrowserContext returns a KeyedService hence the static_cast<>
  // to return a pointer to MediaRouter.
  return static_cast<MediaRouter*>(
      g_instance->GetServiceForBrowserContext(context, true));
}

// static
MediaRouter* MediaRouterFactory::GetApiForBrowserContextIfExists(
    BrowserContext* context) {
  if (!context) {
    return nullptr;
  }
  return static_cast<MediaRouter*>(
      g_instance->GetServiceForBrowserContext(context, false));
}

// static
MediaRouterFactory* MediaRouterFactory::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

MediaRouterFactory::MediaRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DCHECK(!g_instance);
  g_instance = this;
}

MediaRouterFactory::~MediaRouterFactory() {
  g_instance = nullptr;
}

}  // namespace media_router
