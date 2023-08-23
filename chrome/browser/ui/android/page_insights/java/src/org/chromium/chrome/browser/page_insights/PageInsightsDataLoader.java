// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import org.chromium.chrome.browser.page_insights.proto.PageInsights.AutoPeekConditions;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;

/**
 *  Class to provide a {@link PageInsights} data and helper methods
 */
public class PageInsightsDataLoader {
    private float mConfidence = 0.51f;

    private PageInsightsMetadata mPageInsightsMetadata;

    public PageInsightsDataLoader() {
        Page childPage = Page.newBuilder()
                                 .setId(Page.PageID.PEOPLE_ALSO_VIEW)
                                 .setTitle("People also view")
                                 .build();
        Page feedPage = Page.newBuilder()
                                .setId(Page.PageID.SINGLE_FEED_ROOT)
                                .setTitle("Related Insights")
                                .build();
        AutoPeekConditions mAutoPeekConditions = AutoPeekConditions.newBuilder()
                                                         .setConfidence(mConfidence)
                                                         .setPageScrollFraction(0.4f)
                                                         .setMinimumSecondsOnPage(30)
                                                         .build();
        mPageInsightsMetadata = PageInsightsMetadata.newBuilder()
                                        .setFeedPage(feedPage)
                                        .addPages(childPage)
                                        .setAutoPeekConditions(mAutoPeekConditions)
                                        .build();
    }

    PageInsightsDataLoader loadInsightsData() {
        // TODO(mako): Fetch page insights real data
        return this;
    }

    PageInsightsMetadata getData() {
        return mPageInsightsMetadata;
    }

    void setConfidenceForTesting(float confidence) {
        // TODO(mako): Return real confidence
        mPageInsightsMetadata =
                mPageInsightsMetadata.toBuilder()
                        .setAutoPeekConditions(mPageInsightsMetadata.getAutoPeekConditions()
                                                       .toBuilder()
                                                       .setConfidence(confidence)
                                                       .build())
                        .build();
    }
}
