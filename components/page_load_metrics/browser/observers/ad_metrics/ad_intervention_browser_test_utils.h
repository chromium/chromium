// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AD_INTERVENTION_BROWSER_TEST_UTILS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AD_INTERVENTION_BROWSER_TEST_UTILS_H_

class GURL;

namespace content {
class WebContents;
}

// Navigates to |url| in |web_contents| and waits for the first contentful
// paint.
void NavigateAndWaitForFirstContentfulPaint(content::WebContents* web_contents,
                                            const GURL& url);

// Creates a large sticky ad in |web_contents| and triggers a series of actions
// and layout updates for the ad to be detected by the sticky ad detector.
void TriggerAndDetectLargeStickyAd(content::WebContents* web_contents);

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_AD_INTERVENTION_BROWSER_TEST_UTILS_H_
