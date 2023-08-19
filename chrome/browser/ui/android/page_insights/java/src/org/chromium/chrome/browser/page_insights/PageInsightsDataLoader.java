// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

/**
 *  Class to provide a {@link PageInsights} data and helper methods
 */
public class PageInsightsDataLoader {
    private float mConfidence = 0.51f;

    public PageInsightsDataLoader() {}

    PageInsightsDataLoader loadInsightsData() {
        // TODO(mako): Fetch page insights real data
        return this;
    }

    float getConfidence() {
        // TODO(mako): Return real confidence
        return mConfidence;
    }

    void setConfidenceForTesting(float confidence) {
        // TODO(mako): Return real confidence
        mConfidence = confidence;
    }
}
