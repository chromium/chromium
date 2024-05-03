// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType.PAGE_INSIGHTS;

import android.util.LruCache;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.page_insights.proto.PageInsights;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.RequestContext;
import org.chromium.components.optimization_guide.proto.HintsProto.PageInsightsHubRequestContextMetadata;
import org.chromium.components.optimization_guide.proto.HintsProto.RequestContextMetadata;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Locale;

/** Class to provide a {@link PageInsights} data and helper methods */
class PageInsightsDataLoader {

    @VisibleForTesting
    static final String PAGE_INSIGHTS_SEND_TIMESTAMP = "page_insights_send_timestamp";

    private static final String TAG = "PIDataLoader";
    private static final int LRU_CACHE_SIZE = 10;

    private final Supplier<Profile> mProfileSupplier;
    private final boolean mSendTimestamp =
            ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, PAGE_INSIGHTS_SEND_TIMESTAMP, false);
    private LruCache<GURL, PageInsightsMetadata> mCache =
            new LruCache<GURL, PageInsightsMetadata>(LRU_CACHE_SIZE);
    private OptimizationGuideBridge mOptimizationGuideBridge;
    @Nullable private Callback<PageInsightsMetadata> mCurrentCallback;

    PageInsightsDataLoader(Supplier<Profile> profileSupplier) {
        mProfileSupplier = profileSupplier;
    }

    /**
     * Fetches insights data for the given {@code url}, or retrieves from cache if present, and runs
     * {@code callback} with the retrieved data.
     *
     * <p>If called a second time, before first fetch has completed, first callback will not be run.
     */
    void loadInsightsData(
            GURL url,
            boolean isUserInitiated,
            PageInsightsConfig config,
            Callback<PageInsightsMetadata> callback) {
        mCurrentCallback = callback;
        if (url == null) {
            Log.e(TAG, "Error fetching Page Insights data: Url cannot be null.");
            return;
        }
        if (mCache.get(url) != null) {
            callback.bind(mCache.get(url)).run();
            return;
        }

        PageInsightsHubRequestContextMetadata.Builder requestContextMetadataBuilder =
                PageInsightsHubRequestContextMetadata.newBuilder();
        requestContextMetadataBuilder
                .setIsUserInitiated(isUserInitiated)
                .setIsInitialPage(config.getIsInitialPage())
                .setShouldNotLogOrPersonalize(config.getServerShouldNotLogOrPersonalize());
        if (mSendTimestamp && config.hasNavigationTimestampMs()) {
            requestContextMetadataBuilder.setNavigationTimestampMs(
                    config.getNavigationTimestampMs());
        }

        assert mProfileSupplier.hasValue() : "Attempting to load data without a Profile";
        if (mOptimizationGuideBridge == null) {
            mOptimizationGuideBridge =
                    OptimizationGuideBridgeFactory.getForProfile(
                            mProfileSupplier.get().getOriginalProfile());
            if (mOptimizationGuideBridge == null) return;
        }

        mOptimizationGuideBridge.canApplyOptimizationOnDemand(
                List.of(url),
                List.of(PAGE_INSIGHTS),
                RequestContext.CONTEXT_PAGE_INSIGHTS_HUB,
                (gurl, optimizationType, decision, metadata) -> {
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
                },
                RequestContextMetadata.newBuilder()
                        .setPageInsightsHubMetadata(requestContextMetadataBuilder)
                        .build());
    }

    void clearCacheForTesting() {
        mCache = new LruCache<GURL, PageInsightsMetadata>(LRU_CACHE_SIZE);
    }

    void cancelCallback() {
        mCurrentCallback = null;
    }
}
