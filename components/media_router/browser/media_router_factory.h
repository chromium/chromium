// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_FACTORY_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace media_router {

class MediaRouter;

// A factory that lazily returns a MediaRouter implementation for a given
// BrowserContext.
class MediaRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  MediaRouterFactory(const MediaRouterFactory&) = delete;
  MediaRouterFactory& operator=(const MediaRouterFactory&) = delete;

  static MediaRouter* GetApiForBrowserContext(content::BrowserContext* context);

  static MediaRouterFactory* GetInstance();

 protected:
  // We override the shutdown method for the factory to give the Media Router a
  // chance to remove incognito media routes.
  void BrowserContextShutdown(content::BrowserContext* context) override;

  MediaRouterFactory();
  ~MediaRouterFactory() override;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_FACTORY_H_
