// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_e2e_browsertest.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

// Use the following command to run e2e browser tests:
// ./out/Debug/browser_tests --user-data-dir=<empty user data dir>
//   --extension-unpacked=<mr extension dir>
//   --receiver=<chromecast device name>
//   --enable-pixel-output-in-tests --run-manual
//   --gtest_filter=MediaRouterE2EBrowserTest.<test case name>
//   --enable-logging=stderr
//   --ui-test-action-timeout=200000

namespace {
// URL to launch Castv2Player_Staging app on Chromecast
const char kCastAppPresentationUrl[] =
    "cast:BE6E4473?clientId=143692175507258981";
const char kVideo[] = "video";
const char kBearVP9Video[] = "bear-vp9.webm";
const char kPlayer[] = "player.html";
const char kOrigin[] = "http://origin/";
}  // namespace

namespace media_router {

MediaRouterE2EBrowserTest::MediaRouterE2EBrowserTest()
    : media_router_(nullptr) {}

MediaRouterE2EBrowserTest::~MediaRouterE2EBrowserTest() {}

void MediaRouterE2EBrowserTest::SetUpOnMainThread() {
  MediaRouterBaseBrowserTest::SetUpOnMainThread();
  media_router_ =
      MediaRouterFactory::GetApiForBrowserContext(browser()->profile());
  DCHECK(media_router_);
}

void MediaRouterE2EBrowserTest::TearDownOnMainThread() {
  observer_.reset();
  route_id_.clear();
  media_router_ = nullptr;
  MediaRouterBaseBrowserTest::TearDownOnMainThread();
}

void MediaRouterE2EBrowserTest::OnRouteResponseReceived(
    mojom::RoutePresentationConnectionPtr,
    const RouteRequestResult& result) {
  ASSERT_TRUE(result.route());
  route_id_ = result.route()->media_route_id();
}

void MediaRouterE2EBrowserTest::CreateMediaRoute(
    const MediaSource& source,
    const url::Origin& origin,
    content::WebContents* web_contents) {
  DCHECK(media_router_);
  observer_.reset(new TestMediaSinksObserver(media_router_, source, origin));
  observer_->Init();

  DVLOG(1) << "Receiver name: " << receiver_;
  // Wait for MediaSinks compatible with |source| to be discovered.
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(30), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterE2EBrowserTest::IsSinkDiscovered,
                 base::Unretained(this))));

  const auto& sink_map = observer_->sink_map;
  const auto it = sink_map.find(receiver_);
  const MediaSink& sink = it->second;

  // The callback will set route_id_ when invoked.
  media_router_->CreateRoute(
      source.id(), sink.id(), origin, web_contents,
      base::BindOnce(&MediaRouterE2EBrowserTest::OnRouteResponseReceived,
                     base::Unretained(this)),
      base::TimeDelta(), is_incognito());

  // Wait for the route request to be fulfilled (and route to be started).
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(30), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterE2EBrowserTest::IsRouteCreated,
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
  GURL gurl =
      content::GetFileUrlWithQuery(media::GetTestDataFilePath(kPlayer), query);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), gurl, 1);
}

// Test cases

IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest, MANUAL_TabMirroring) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SessionID tab_id = SessionTabHelper::IdForTab(web_contents);

  // Wait for 30 seconds to make sure the route is stable.
  CreateMediaRoute(MediaSource::ForTab(tab_id.id()),
                   url::Origin::Create(GURL(kOrigin)), web_contents);
  Wait(base::TimeDelta::FromSeconds(30));

  // Wait for 10 seconds to make sure route has been stopped.
  StopMediaRoute();
  Wait(base::TimeDelta::FromSeconds(10));
}

IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest, MANUAL_CastApp) {
  // Wait for 30 seconds to make sure the route is stable.
  CreateMediaRoute(
      MediaSource::ForPresentationUrl(GURL(kCastAppPresentationUrl)),
      url::Origin::Create(GURL(kOrigin)), nullptr);
  Wait(base::TimeDelta::FromSeconds(30));

  // Wait for 10 seconds to make sure route has been stopped.
  StopMediaRoute();
  Wait(base::TimeDelta::FromSeconds(10));
}

}  // namespace media_router
