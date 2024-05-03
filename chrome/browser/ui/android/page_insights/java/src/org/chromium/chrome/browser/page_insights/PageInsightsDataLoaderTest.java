// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
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

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactoryJni;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.AutoPeekConditions;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.components.optimization_guide.proto.HintsProto.PageInsightsHubRequestContextMetadata;
import org.chromium.components.optimization_guide.proto.HintsProto.RequestContextMetadata;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/** Unit tests for {@link PageInsightsDataLoader}. */
@LooperMode(LooperMode.Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public class PageInsightsDataLoaderTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private OptimizationGuideBridgeFactory.Natives mOptimizationGuideBridgeFactoryJniMock;
    @Mock private OptimizationGuideBridge mOptimizationGuideBridge;

    @Mock private Profile mProfile;

    private PageInsightsDataLoader mPageInsightsDataLoader;

    private PageInsightsMetadata mPageInsightsMetadata = pageInsights();

    private final GURL mUrl = JUnitTestGURLs.EXAMPLE_URL;

    private final GURL mUrl2 = JUnitTestGURLs.GOOGLE_URL;

    private OptimizationGuideBridge.OnDemandOptimizationGuideCallback mOptimizationGuideCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(
                OptimizationGuideBridgeFactoryJni.TEST_HOOKS,
                mOptimizationGuideBridgeFactoryJniMock);
        doReturn(mOptimizationGuideBridge)
                .when(mOptimizationGuideBridgeFactoryJniMock)
                .getForProfile(mProfile);
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        mockOptimizationGuideResponse(
                mUrl, anyPageInsights(mPageInsightsMetadata), OptimizationGuideDecision.TRUE);
        createDataLoader();
    }

    private void createDataLoader() {
        createDataLoader(new TestValues());
    }

    private void createDataLoader(TestValues testValues) {
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        FeatureList.mergeTestValues(testValues, /* replace= */ true);
        ObservableSupplierImpl<Profile> profileSupplier = new ObservableSupplierImpl<>();
        profileSupplier.set(mProfile);
        mPageInsightsDataLoader = new PageInsightsDataLoader(profileSupplier);
    }

    @Test
    public void testLoadInsightsData_returnsCorrectData() {
        mPageInsightsDataLoader.clearCacheForTesting();

        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* isUserInitiated= */ true,
                PageInsightsConfig.getDefaultInstance(),
                (data) -> {
                    assertEquals(data, mPageInsightsMetadata);
                });
    }

    @Test
    public void testLoadInsightsData_cancelCallback_callbackNotExecuted() {
        mockOptimizationGuideResponse(
                mUrl,
                anyPageInsights(mPageInsightsMetadata),
                OptimizationGuideDecision.TRUE,
                /* shouldRunCallbackImmediately= */ false);
        mPageInsightsDataLoader.clearCacheForTesting();

        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* isUserInitiated= */ true,
                PageInsightsConfig.getDefaultInstance(),
                (data) -> {
                    fail("Callback should not have been called after cancelled.");
                });
        mPageInsightsDataLoader.cancelCallback();
        mOptimizationGuideCallback.onOnDemandOptimizationGuideDecision(
                mUrl,
                HintsProto.OptimizationType.PAGE_INSIGHTS,
                OptimizationGuideDecision.TRUE,
                anyPageInsights(mPageInsightsMetadata));
    }

    @Test
    public void testLoadInsightsData_optimizationGuideDecisionFalse_returnsNull() {
        mPageInsightsDataLoader.clearCacheForTesting();
        mockOptimizationGuideResponse(
                mUrl, anyPageInsights(mPageInsightsMetadata), OptimizationGuideDecision.FALSE);

        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* isUserInitiated= */ true,
                PageInsightsConfig.getDefaultInstance(),
                (data) -> {
                    assertNull(data);
                });
    }

    @Test
    public void testLoadInsightsData_nullUrl_returnsNull() {
        mPageInsightsDataLoader.clearCacheForTesting();
        mPageInsightsDataLoader.loadInsightsData(
                null,
                /* isUserInitiated= */ true,
                PageInsightsConfig.getDefaultInstance(),
                (data) -> {
                    assertNull(data);
                });
    }

    @Test
    public void testLoadInsightsData_sendTimestamp_sendsMetadataAndAllowsGaia() {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PageInsightsDataLoader.PAGE_INSIGHTS_SEND_TIMESTAMP,
                "true");
        createDataLoader(testValues);

        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* isUserInitiated= */ true,
                PageInsightsConfig.newBuilder()
                        .setIsInitialPage(true)
                        .setServerShouldNotLogOrPersonalize(true)
                        .setNavigationTimestampMs(1234L)
                        .build(),
                (data) -> {});

        RequestContextMetadata expectedMetadata =
                RequestContextMetadata.newBuilder()
                        .setPageInsightsHubMetadata(
                                PageInsightsHubRequestContextMetadata.newBuilder()
                                        .setIsUserInitiated(true)
                                        .setIsInitialPage(true)
                                        .setShouldNotLogOrPersonalize(true)
                                        .setNavigationTimestampMs(1234L))
                        .build();
        verify(mOptimizationGuideBridge, times(1))
                .canApplyOptimizationOnDemand(
                        eq(Arrays.asList(mUrl)),
                        eq(Arrays.asList(HintsProto.OptimizationType.PAGE_INSIGHTS)),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class),
                        eq(expectedMetadata));
    }

    @Test
    public void testLoadInsightsData_doNotSendTimestamp_sendsMetadataAndAllowsGaia() {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PageInsightsDataLoader.PAGE_INSIGHTS_SEND_TIMESTAMP,
                "false");
        createDataLoader(testValues);

        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* isUserInitiated= */ true,
                PageInsightsConfig.newBuilder()
                        .setIsInitialPage(true)
                        .setServerShouldNotLogOrPersonalize(true)
                        .setNavigationTimestampMs(1234L)
                        .build(),
                (data) -> {});

        RequestContextMetadata expectedMetadata =
                RequestContextMetadata.newBuilder()
                        .setPageInsightsHubMetadata(
                                PageInsightsHubRequestContextMetadata.newBuilder()
                                        .setIsUserInitiated(true)
                                        .setIsInitialPage(true)
                                        .setShouldNotLogOrPersonalize(true))
                        .build();
        verify(mOptimizationGuideBridge, times(1))
                .canApplyOptimizationOnDemand(
                        eq(Arrays.asList(mUrl)),
                        eq(Arrays.asList(HintsProto.OptimizationType.PAGE_INSIGHTS)),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class),
                        eq(expectedMetadata));
    }

    @Test
    public void testLoadInsightsData_sameUrl_doesNotCallOptimizationGuideBridge() {
        mPageInsightsDataLoader.clearCacheForTesting();
        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* isUserInitiated= */ true,
                PageInsightsConfig.getDefaultInstance(),
                (data) -> {});

        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* isUserInitiated= */ true,
                PageInsightsConfig.getDefaultInstance(),
                (data) -> {});

        verify(mOptimizationGuideBridge, times(1))
                .canApplyOptimizationOnDemand(
                        eq(Arrays.asList(mUrl)),
                        eq(Arrays.asList(HintsProto.OptimizationType.PAGE_INSIGHTS)),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class),
                        any());
    }

    @Test
    public void testLoadInsightsData_differentUrls_callsOptimizationGuideBridge() {
        mPageInsightsDataLoader.clearCacheForTesting();
        mPageInsightsDataLoader.loadInsightsData(
                mUrl,
                /* isUserInitiated= */ true,
                PageInsightsConfig.getDefaultInstance(),
                (data) -> {});

        mPageInsightsDataLoader.loadInsightsData(
                mUrl2,
                /* isUserInitiated= */ true,
                PageInsightsConfig.getDefaultInstance(),
                (data) -> {});

        verify(mOptimizationGuideBridge, times(1))
                .canApplyOptimizationOnDemand(
                        eq(Arrays.asList(mUrl)),
                        eq(Arrays.asList(HintsProto.OptimizationType.PAGE_INSIGHTS)),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class),
                        any());
        verify(mOptimizationGuideBridge, times(1))
                .canApplyOptimizationOnDemand(
                        eq(Arrays.asList(mUrl2)),
                        eq(Arrays.asList(HintsProto.OptimizationType.PAGE_INSIGHTS)),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class),
                        any());
    }

    private void mockOptimizationGuideResponse(
            GURL url, CommonTypesProto.Any metadata, int decision) {
        mockOptimizationGuideResponse(
                url, metadata, decision, /* shouldRunCallbackImmediately= */ true);
    }

    private void mockOptimizationGuideResponse(
            GURL url,
            CommonTypesProto.Any metadata,
            int decision,
            boolean shouldRunCallbackImmediately) {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                mOptimizationGuideCallback =
                                        (OptimizationGuideBridge.OnDemandOptimizationGuideCallback)
                                                invocation.getArguments()[3];
                                if (shouldRunCallbackImmediately) {
                                    mOptimizationGuideCallback.onOnDemandOptimizationGuideDecision(
                                            url,
                                            HintsProto.OptimizationType.PAGE_INSIGHTS,
                                            decision,
                                            metadata);
                                }
                                return null;
                            }
                        })
                .when(mOptimizationGuideBridge)
                .canApplyOptimizationOnDemand(
                        any(),
                        any(),
                        any(CommonTypesProto.RequestContext.class),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class),
                        any());
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
