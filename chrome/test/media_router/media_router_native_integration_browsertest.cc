// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "chrome/browser/media/router/mojo/media_router_desktop.h"
#include "chrome/browser/media/router/providers/test/test_media_route_provider.h"
#include "chrome/test/media_router/media_router_integration_browsertest.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

// TODO(crbug.com/1152593): Merge this class back into
// MediaRouterIntegrationBrowserTest once all the test cases have been converted
// to use the native test MRP instead of the extension MRP. Then the extension
// setup code in MediaRouterBaseBrowserTest can also be deleted.
class MediaRouterNativeIntegrationBrowserTest
    : public MediaRouterIntegrationBrowserTest {
 public:
  MediaRouterNativeIntegrationBrowserTest() = default;
  ~MediaRouterNativeIntegrationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // We don't call super::SetUpOnMainThread() here so that we're not setting
    // up the extension test MRP.
    MediaRouterMojoImpl* router = static_cast<MediaRouterMojoImpl*>(
        MediaRouterFactory::GetApiForBrowserContext(browser()->profile()));
    mojo::PendingRemote<mojom::MediaRouter> media_router_remote;
    mojo::PendingRemote<mojom::MediaRouteProvider> provider_remote;
    router->BindToMojoReceiver(
        media_router_remote.InitWithNewPipeAndPassReceiver());
    test_provider_ = std::make_unique<TestMediaRouteProvider>(
        provider_remote.InitWithNewPipeAndPassReceiver(),
        std::move(media_router_remote));
    router->RegisterMediaRouteProvider(MediaRouteProviderId::TEST,
                                       std::move(provider_remote),
                                       base::DoNothing());

    test_ui_ =
        MediaRouterUiForTest::GetOrCreateForWebContents(GetActiveWebContents());
  }

  std::unique_ptr<TestMediaRouteProvider> test_provider_;
};

IN_PROC_BROWSER_TEST_F(MediaRouterNativeIntegrationBrowserTest, Basic) {
  RunBasicTest();
}

}  // namespace media_router
