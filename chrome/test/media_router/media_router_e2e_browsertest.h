// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_E2E_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_E2E_BROWSERTEST_H_

#include <memory>
#include <string>

#include "chrome/test/media_router/media_router_integration_browsertest.h"
#include "chrome/test/media_router/test_media_sinks_observer.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/common/media_route.h"

namespace media_router {

class MediaRouter;
class RouteRequestResult;

class MediaRouterE2EBrowserTest : public MediaRouterIntegrationBrowserTest {
 public:
  MediaRouterE2EBrowserTest();
  ~MediaRouterE2EBrowserTest() override;

 protected:
  // InProcessBrowserTest Overrides
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;


  // Callback from MediaRouter when a response to a media route request is
  // received.
  void OnRouteResponseReceived(mojom::RoutePresentationConnectionPtr,
                               const RouteRequestResult& result);

  // Initializes |observer_| to listen for sinks compatible with |source|,
  // finds sink with name matching receiver_, and establishes media
  // route between the source and sink.
  // |observer_| and |route_id_| will be initialized.
  // |origin| is the URL of requestor's page.
  // |web_contents| identifies the tab in which the request was made.
  // |origin| and |web_contents| are used for enforcing same-origin and/or
  // same-tab scope for JoinRoute() requests. (e.g., if enforced, the page
  // requesting JoinRoute() must have the same origin as the page that
  // requested CreateRoute()).
  void CreateMediaRoute(const MediaSource& source,
                        const url::Origin& origin,
                        content::WebContents* web_contents);

  // Stops the established media route and unregisters |observer_|.
  // Note that the route may not be stopped immediately, as it makes an
  // async call to the Media Route Provider.
  // |observer_| and |route_id_| will be reset.
  void StopMediaRoute();

  bool IsSinkDiscovered() const;
  bool IsRouteCreated() const;

  void OpenMediaPage();

 private:
  MediaRouter* media_router_;
  std::unique_ptr<TestMediaSinksObserver> observer_;
  MediaRoute::Id route_id_;
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_E2E_BROWSERTEST_H_
