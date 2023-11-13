// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.AutoPeekConditions;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link PageInsightsDataLoader}. */
@LooperMode(LooperMode.Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public class PageInsightsDataLoaderTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Mock private Profile mProfile;

    private PageInsightsDataLoader mPageInsightsDataLoader;

    private PageInsightsMetadata mPageInsightsMetadata = pageInsights();

    private final GURL mUrl = JUnitTestGURLs.EXAMPLE_URL;

    private final GURL mUrl2 = JUnitTestGURLs.GOOGLE_URL;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();
        Profile.setLastUsedProfileForTesting(mProfile);
        mockOptimizationGuideResponse(
                mUrl, anyPageInsights(mPageInsightsMetadata), OptimizationGuideDecision.TRUE);
        mPageInsightsDataLoader = new PageInsightsDataLoader();
    }

    @Test
    public void testLoadInsightsData_returnsCorrectData() {
        mPageInsightsDataLoader.clearCacheForTesting();

        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* shouldAttachGaiaToRequest= */ true,
                (data) -> {
                    assertEquals(data, mPageInsightsMetadata);
                });
    }

    @Test
    public void testLoadInsightsData_destroyed_callbackNotExecuted() {
        mPageInsightsDataLoader.clearCacheForTesting();

        mPageInsightsDataLoader.destroy();
        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* shouldAttachGaiaToRequest= */ true,
                (data) -> {
                    fail("Callback should not have been called after loader destroyed.");
                });
    }

    @Test
    public void testLoadInsightsData_optimizationGuideDecisionFalse_returnsNull() {
        mPageInsightsDataLoader.clearCacheForTesting();
        mockOptimizationGuideResponse(
                mUrl, anyPageInsights(mPageInsightsMetadata), OptimizationGuideDecision.FALSE);

        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* shouldAttachGaiaToRequest= */ true,
                (data) -> {
                    assertNull(data);
                });
    }

    @Test
    public void testLoadInsightsData_nullUrl_returnsNull() {
        mPageInsightsDataLoader.clearCacheForTesting();
        mPageInsightsDataLoader.loadInsightsData(
                null,
                /* shouldAttachGaiaToRequest= */ true,
                (data) -> {
                    assertNull(data);
                });
    }

    @Test
    public void testLoadInsightsData_emptyCache_gaia_callsOptimizationGuideBridge() {
        mPageInsightsDataLoader.clearCacheForTesting();

        mPageInsightsDataLoader.loadInsightsData(
                mUrl, /* shouldAttachGaiaToRequest= */ true, (data) -> {});

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationOnDemand(
                        eq(1L),
                        eq(new GURL[] {mUrl}),
                        eq(new int[] {HintsProto.OptimizationType.PAGE_INSIGHTS.getNumber()}),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB.getNumber()),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class));
    }

    @Test
    public void testLoadInsightsData_emptyCache_noGaia_callsOptimizationGuideBridge() {
        mPageInsightsDataLoader.clearCacheForTesting();

        mPageInsightsDataLoader.loadInsightsData(
                mUrl, /* shouldAttachGaiaToRequest= */ false, (data) -> {});

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationOnDemand(
                        eq(1L),
                        eq(new GURL[] {mUrl}),
                        eq(new int[] {HintsProto.OptimizationType.PAGE_INSIGHTS.getNumber()}),
                        eq(
                                CommonTypesProto.RequestContext
                                        .CONTEXT_NON_PERSONALIZED_PAGE_INSIGHTS_HUB
                                        .getNumber()),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class));
    }

    @Test
    public void testLoadInsightsData_sameUrl_doesNotCallOptimizationGuideBridge() {
        mPageInsightsDataLoader.clearCacheForTesting();
        mPageInsightsDataLoader.loadInsightsData(
                mUrl, /* shouldAttachGaiaToRequest= */ true, (data) -> {});

        mPageInsightsDataLoader.loadInsightsData(
                mUrl, /* shouldAttachGaiaToRequest= */ true, (data) -> {});

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationOnDemand(
                        eq(1L),
                        eq(new GURL[] {mUrl}),
                        eq(new int[] {HintsProto.OptimizationType.PAGE_INSIGHTS.getNumber()}),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB.getNumber()),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class));
    }

    @Test
    public void testLoadInsightsData_differentUrls_callsOptimizationGuideBridge() {
        mPageInsightsDataLoader.clearCacheForTesting();
        mPageInsightsDataLoader.loadInsightsData(
                mUrl, /* shouldAttachGaiaToRequest= */ true, (data) -> {});

        mPageInsightsDataLoader.loadInsightsData(
                mUrl2, /* shouldAttachGaiaToRequest= */ true, (data) -> {});

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationOnDemand(
                        eq(1L),
                        eq(new GURL[] {mUrl}),
                        eq(new int[] {HintsProto.OptimizationType.PAGE_INSIGHTS.getNumber()}),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB.getNumber()),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class));
        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationOnDemand(
                        eq(1L),
                        eq(new GURL[] {mUrl2}),
                        eq(new int[] {HintsProto.OptimizationType.PAGE_INSIGHTS.getNumber()}),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB.getNumber()),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class));
    }

    private void mockOptimizationGuideResponse(
            GURL url, CommonTypesProto.Any metadata, int decision) {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                OptimizationGuideBridge.OnDemandOptimizationGuideCallback callback =
                                        (OptimizationGuideBridge.OnDemandOptimizationGuideCallback)
                                                invocation.getArguments()[4];
                                callback.onOnDemandOptimizationGuideDecision(
                                        url,
                                        HintsProto.OptimizationType.PAGE_INSIGHTS,
                                        decision,
                                        metadata);
                                return null;
                            }
                        })
                .when(mOptimizationGuideBridgeJniMock)
                .canApplyOptimizationOnDemand(
                        anyLong(),
                        any(),
                        any(),
                        anyInt(),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class));
    }

    private static CommonTypesProto.Any anyPageInsights(PageInsightsMetadata pageInsightsMetaData) {
        return CommonTypesProto.Any.newBuilder()
                .setValue(ByteString.copyFrom(pageInsightsMetaData.toByteArray()))
                .build();
    }

    private static PageInsightsMetadata pageInsights() {
        Page childPage =
                Page.newBuilder()
                        .setId(Page.PageID.PEOPLE_ALSO_VIEW)
                        .setTitle("People also view")
                        .build();
        Page feedPage =
                Page.newBuilder()
                        .setId(Page.PageID.SINGLE_FEED_ROOT)
                        .setTitle("Related Insights")
                        .build();
        AutoPeekConditions mAutoPeekConditions =
                AutoPeekConditions.newBuilder()
                        .setConfidence(.51f)
                        .setPageScrollFraction(0.4f)
                        .setMinimumSecondsOnPage(30)
                        .build();
        return PageInsightsMetadata.newBuilder()
                .setFeedPage(feedPage)
                .addPages(childPage)
                .setAutoPeekConditions(mAutoPeekConditions)
                .build();
    }
}
