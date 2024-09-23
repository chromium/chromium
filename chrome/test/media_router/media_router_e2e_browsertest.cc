// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_e2e_browsertest.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/route_request_result.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {
// URL to launch Castv2Player_Staging app on Chromecast
const char kCastAppPresentationUrl[] =
    "cast:BE6E4473?clientId=143692175507258981";
const char kVideo[] = "video";
const char kBearVP9Video[] = "bear-vp9.webm";
const char kOrigin[] = "http://origin/";
}  // namespace

namespace media_router {

MediaRouterE2EBrowserTest::MediaRouterE2EBrowserTest()
    : media_router_(nullptr) {}

MediaRouterE2EBrowserTest::~MediaRouterE2EBrowserTest() {}

void MediaRouterE2EBrowserTest::SetUpOnMainThread() {
  MediaRouterIntegrationBrowserTest::SetUpOnMainThread();
  media_router_ =
      MediaRouterFactory::GetApiForBrowserContext(browser()->profile());
  DCHECK(media_router_);
// On Mac, cast device discovery isn't started until explicit user gesture.
// Starting sink discovery now for tests.
#if BUILDFLAG(IS_MAC)
  media_router_->OnUserGesture();
#endif
}

void MediaRouterE2EBrowserTest::TearDownOnMainThread() {
  observer_.reset();
  route_id_.clear();
  media_router_ = nullptr;
  MediaRouterIntegrationBrowserTest::TearDownOnMainThread();
}

bool MediaRouterE2EBrowserTest::RequiresMediaRouteProviders() const {
  return true;
}

void MediaRouterE2EBrowserTest::OnRouteResponseReceived(
    mojom::RoutePresentationConnectionPtr,
    const RouteRequestResult& result) {
  ASSERT_TRUE(result.route())
      << "RouteRequestResult code: " << result.result_code();
  route_id_ = result.route()->media_route_id();
}

void MediaRouterE2EBrowserTest::CreateMediaRoute(
    const MediaSource& source,
    const url::Origin& origin,
    content::WebContents* web_contents) {
  DCHECK(media_router_);
  observer_ =
      std::make_unique<TestMediaSinksObserver>(media_router_, source, origin);
  observer_->Init();

  DVLOG(1) << "Receiver name: " << receiver_;
  // Wait for MediaSinks compatible with |source| to be discovered.  Waiting is
  // needed here because tests that use this method are always executed using a
  // real device, as opposed to a fake device provided by the test MRP.
  ASSERT_TRUE(ConditionalWait(
      base::Seconds(60), base::Seconds(1),
      base::BindRepeating(&MediaRouterE2EBrowserTest::IsSinkDiscovered,
                          base::Unretained(this))));

  const auto& sink_map = observer_->sink_map;
  const auto it = sink_map.find(receiver_);
  const MediaSink& sink = it->second;

  // The callback will set route_id_ when invoked.
  media_router_->CreateRoute(
      source.id(), sink.id(), origin, web_contents,
      base::BindOnce(&MediaRouterE2EBrowserTest::OnRouteResponseReceived,
                     base::Unretained(this)),
      base::TimeDelta());

  // Wait for the route request to be fulfilled (and route to be started).
  ASSERT_TRUE(ConditionalWait(
      base::Seconds(60), base::Seconds(1),
      base::BindRepeating(&MediaRouterE2EBrowserTest::IsRouteCreated,
                          base::Unretained(this))));
}

void MediaRouterE2EBrowserTest::StopMediaRoute() {
  ASSERT_FALSE(route_id_.empty());
  media_router_->TerminateRoute(route_id_);
}

bool MediaRouterE2EBrowserTest::IsSinkDiscovered() const {
  return base::Contains(observer_->sink_map, receiver_);
}

bool MediaRouterE2EBrowserTest::IsRouteCreated() const {
  return !route_id_.empty();
}

void MediaRouterE2EBrowserTest::OpenMediaPage() {
  base::StringPairs query_params;
  query_params.push_back(std::make_pair(kVideo, kBearVP9Video));
  std::string query = media::GetURLQueryString(query_params);
  GURL gurl = content::GetFileUrlWithQuery(
      GetResourceFile(FILE_PATH_LITERAL("player.html")), query);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), gurl, 1);
}

// Test cases

IN_PROC_BROWSER_TEST_P(MediaRouterE2EBrowserTest, MANUAL_TabMirroring) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);

  // Wait for 30 seconds to make sure the route is stable.
  CreateMediaRoute(MediaSource::ForTab(tab_id.id()),
                   url::Origin::Create(GURL(kOrigin)), web_contents);
  Wait(base::Seconds(30));

  // Wait for 10 seconds to make sure route has been stopped.
  StopMediaRoute();
  Wait(base::Seconds(10));
}

IN_PROC_BROWSER_TEST_P(MediaRouterE2EBrowserTest, MANUAL_CastApp) {
  // Wait for 30 seconds to make sure the route is stable.
  CreateMediaRoute(
      MediaSource::ForPresentationUrl(GURL(kCastAppPresentationUrl)),
      url::Origin::Create(GURL(kOrigin)), nullptr);
  Wait(base::Seconds(30));

  // Wait for 10 seconds to make sure route has been stopped.
  StopMediaRoute();
  Wait(base::Seconds(10));
}

INSTANTIATE_MEDIA_ROUTER_INTEGRATION_BROWER_TEST_SUITE(
    MediaRouterE2EBrowserTest);

}  // namespace media_router
