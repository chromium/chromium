// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/ad_intervention_browser_test_utils.h"

#include <memory>
#include <string>

#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_tree_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace page_load_metrics {

namespace {

// Scales the rect by the web content's render widget host's device scale
// factor.
gfx::Rect ScaleRectByDeviceScaleFactor(const gfx::Rect& rect,
                                       content::WebContents* web_contents) {
  return gfx::ScaleToRoundedRect(
      rect, web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor());
}

// Navigates to |url| in |web_contents| and waits for |event|.
void NavigateAndWaitForTimingEvent(
    content::WebContents* web_contents,
    const GURL& url,
    PageLoadMetricsTestWaiter::TimingField event) {
  auto waiter = std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  waiter->AddPageExpectation(event);

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));
  waiter->Wait();
  waiter.reset();
}

}  // namespace

const char kHttpOkResponseHeader[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n";

const int kMaxHeavyAdNetworkSize =
    heavy_ad_thresholds::kMaxNetworkBytes +
    AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider::
        kMaxNetworkThresholdNoiseBytes;

// Gets the body height of the document embedded in |web_contents|.
int GetDocumentHeight(content::WebContents* web_contents) {
  return EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
}

void CreateAndWaitForIframeAtRect(content::WebContents* web_contents,
                                  PageLoadMetricsTestWaiter* waiter,
                                  const GURL& url,
                                  const gfx::Rect& rect) {
  // The intersections returned by the renderer are scaled to the device's
  // scale factor.
  gfx::Rect scaled_rect = ScaleRectByDeviceScaleFactor(rect, web_contents);

  // The renderer propagates values scaled by the device scale factor.
  // Wait on these values.
  waiter->AddMainFrameIntersectionExpectation(scaled_rect);

  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace("let frame = createAdIframeAtRect($1, $2, $3, $4); "
                         "frame.src = $5",
                         rect.x(), rect.y(), rect.width(), rect.height(),
                         url.spec())));

  waiter->Wait();
}

// Navigate to |url| in |web_contents| and wait until we see the first
// contentful paint.
void NavigateAndWaitForFirstContentfulPaint(content::WebContents* web_contents,
                                            const GURL& url) {
  NavigateAndWaitForTimingEvent(
      web_contents, url,
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
}

void NavigateAndWaitForFirstMeaningfulPaint(content::WebContents* web_contents,
                                            const GURL& url) {
  NavigateAndWaitForTimingEvent(
      web_contents, url,
      PageLoadMetricsTestWaiter::TimingField::kFirstMeaningfulPaint);
}

// Create a large sticky ad in |web_contents| and trigger a series of actions
// and layout updates for the ad to be detected by the sticky ad detector.
void TriggerAndDetectLargeStickyAd(content::WebContents* web_contents) {
  // Create the large-sticky-ad.
  EXPECT_TRUE(ExecJs(
      web_contents,
      "let frame = createStickyAdIframeAtBottomOfViewport(window.innerWidth, "
      "window.innerHeight * 0.35);"));

  // Force a layout update to capture the initial state. Then scroll further
  // down.
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(web_contents, "", "window.scrollTo(0, 5000)")
          .error.empty());

  // Force a layout update to capture the final state. At this point the
  // detector should have detected the large-sticky-ad.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(web_contents, "", "").error.empty());
}

void TriggerAndDetectOverlayPopupAd(content::WebContents* web_contents) {
  // Force a layout update to capture the initial state without the ad. Then
  // create the overlay-popup-ad.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                  web_contents, "",
                  "createAdIframeAtRect(window.innerWidth * "
                  "0.25, window.innerHeight * 0.25, window.innerWidth * 0.5, "
                  "window.innerHeight * 0.5)",
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                  .error.empty());

  // Force a layout update to capture the overlay-popup-ad. Then dismiss the
  // ad.
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(web_contents, "",
                                 "document.getElementsByTagName('iframe')[0]."
                                 "style.display = 'none';",
                                 content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .error.empty());

  // Force a layout update to capture the state after the dismissal. At this
  // point the detector should have detected the overlay-popup-ad.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                  web_contents, "", "", content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                  .error.empty());
}

void LoadLargeResource(net::test_server::ControllableHttpResponse* response,
                       int bytes) {
  response->WaitForRequest();
  response->Send(kHttpOkResponseHeader);
  response->Send(std::string(bytes, ' '));
  response->Done();
}

}  // namespace page_load_metrics
