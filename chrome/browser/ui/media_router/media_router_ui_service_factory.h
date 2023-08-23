// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_FACTORY_H_

#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace media_router {

class MediaRouterUIService;

class MediaRouterUIServiceFactory : public ProfileKeyedServiceFactory {
 public:
  MediaRouterUIServiceFactory(const MediaRouterUIServiceFactory&) = delete;
  MediaRouterUIServiceFactory& operator=(const MediaRouterUIServiceFactory&) =
      delete;

  static MediaRouterUIService* GetForBrowserContext(
      content::BrowserContext* context);

  static MediaRouterUIServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<MediaRouterUIServiceFactory>;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUIServiceFactoryUnitTest, CreateService);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUIServiceFactoryUnitTest,
                           DoNotCreateActionControllerWhenDisabled);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUIServiceFactoryUnitTest,
                           DisablingMediaRouting);

  MediaRouterUIServiceFactory();
  ~MediaRouterUIServiceFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
#if !BUILDFLAG(IS_ANDROID)
  bool ServiceIsCreatedWithBrowserContext() const override;
#endif
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_FACTORY_H_
