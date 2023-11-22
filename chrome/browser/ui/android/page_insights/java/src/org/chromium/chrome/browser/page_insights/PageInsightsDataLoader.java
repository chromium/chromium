// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType.PAGE_INSIGHTS;

import android.util.LruCache;

import androidx.annotation.Nullable;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.page_insights.proto.PageInsights;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.RequestContext;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Locale;

/** Class to provide a {@link PageInsights} data and helper methods */
class PageInsightsDataLoader {
    private static final String TAG = "PIDataLoader";
    private static final int LRU_CACHE_SIZE = 10;
    private LruCache<GURL, PageInsightsMetadata> mCache =
            new LruCache<GURL, PageInsightsMetadata>(LRU_CACHE_SIZE);
    private boolean mIsDestroyed;
    @Nullable private Callback<PageInsightsMetadata> mCurrentCallback;

    PageInsightsDataLoader() {}

    /**
     * Fetches insights data for the given {@code url}, or retrieves from cache if present, and runs
     * {@code callback} with the retrieved data.
     *
     * <p>If called a second time, before first fetch has completed, first callback will not be run.
     */
    void loadInsightsData(
            GURL url, boolean shouldAttachGaiaToRequest, Callback<PageInsightsMetadata> callback) {
        mCurrentCallback = callback;
        if (url == null) {
            Log.e(TAG, "Error fetching Page Insights data: Url cannot be null.");
            return;
        }
        if (mCache.get(url) != null) {
            callback.bind(mCache.get(url)).run();
            return;
        }
        OptimizationGuideBridgeFactoryHolder.sOptimizationGuideBridgeFactory
                .create()
                .canApplyOptimizationOnDemand(
                        List.of(url),
                        List.of(PAGE_INSIGHTS),
                        shouldAttachGaiaToRequest
                                ? RequestContext.CONTEXT_PAGE_INSIGHTS_HUB
                                : RequestContext.CONTEXT_NON_PERSONALIZED_PAGE_INSIGHTS_HUB,
                        (gurl, optimizationType, decision, metadata) -> {
                            if (mIsDestroyed) return;
                            try {
                                if (decision != OptimizationGuideDecision.TRUE) {
                                    return;
                                }
                                PageInsightsMetadata pageInsightsMetadata =
                                        PageInsightsMetadata.parseFrom(metadata.getValue());
                                mCache.put(url, pageInsightsMetadata);
                                // There is never a case where we want to run an old callback when
                                // a newer one has been provided, and doing so could lead to bugs
                                // (e.g. peek is shown with insights for previous page).
                                if (mCurrentCallback == callback) {
                                    callback.bind(pageInsightsMetadata).run();
                                }
                            } catch (InvalidProtocolBufferException e) {
                                Log.e(
                                        TAG,
                                        String.format(
                                                Locale.US,
                                                "There was a problem "
                                                        + "parsing "
                                                        + "PageInsightsMetadata"
                                                        + "proto. "
                                                        + "Details %s.",
                                                e));
                            }
                        });
    }

    void clearCacheForTesting() {
        mCache = new LruCache<GURL, PageInsightsMetadata>(LRU_CACHE_SIZE);
    }

    void destroy() {
        mIsDestroyed = true;
    }

    // Lazy initialization of OptimizationGuideBridgeFactory
    private static class OptimizationGuideBridgeFactoryHolder {
        private static final OptimizationGuideBridgeFactory sOptimizationGuideBridgeFactory;

        static {
            sOptimizationGuideBridgeFactory = new OptimizationGuideBridgeFactory();
        }
    }
}
