// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AD_INTERVENTION_BROWSER_TEST_UTILS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AD_INTERVENTION_BROWSER_TEST_UTILS_H_

#include "base/byte_count.h"
#include "net/test/embedded_test_server/controllable_http_response.h"

class GURL;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace net::test_server {
class ControllableHttpResponse;
}

namespace page_load_metrics {

// The header for a 200 response to an HTTP request.
extern const char kHttpOkResponseHeader[];

// The maximum possible threshold beyond which an ad resource will be classified
// as heavy from the network perspective.
extern const base::ByteCount kMaxHeavyAdNetworkSize;

class PageLoadMetricsTestWaiter;

// Gets the body height of the document embedded in |web_contents|.
int GetDocumentHeight(content::WebContents* web_contents);

// Adds a non-trivial HTML paragraph (<p>) element to a blank page, and waits
// for the First Contentful Paint.
void AddTextAndWaitForFirstContentfulPaint(content::WebContents* web_contents,
                                           PageLoadMetricsTestWaiter* waiter);

// Creates an iframe in |web_contents| at |rect| and waits for it to intersect
// the main frame.
void CreateAndWaitForIframeAtRect(content::WebContents* web_contents,
                                  PageLoadMetricsTestWaiter* waiter,
                                  const GURL& url,
                                  const gfx::Rect& rect);

// Navigates to |url| in |web_contents| and waits for the first contentful
// paint.
void NavigateAndWaitForFirstContentfulPaint(content::WebContents* web_contents,
                                            const GURL& url);

// Navigates to |url| in |web_contents| and waits for the first meaningful
// paint.
void NavigateAndWaitForFirstMeaningfulPaint(content::WebContents* web_contents,
                                            const GURL& url);

// Creates a large sticky ad in |web_contents| and triggers a series of actions
// and layout updates for the ad to be detected by the sticky ad detector.
void TriggerAndDetectLargeStickyAd(content::WebContents* web_contents);

// Creates an overlay popup ad and trigger a series of actions and layout
// updates for the ad to be detected by the overlay popup detector.
void TriggerAndDetectOverlayPopupAd(content::WebContents* web_contents);

// Loads a resource of size |bytes| in |response|.
void LoadLargeResource(net::test_server::ControllableHttpResponse* response,
                       base::ByteCount bytes);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AD_INTERVENTION_BROWSER_TEST_UTILS_H_
