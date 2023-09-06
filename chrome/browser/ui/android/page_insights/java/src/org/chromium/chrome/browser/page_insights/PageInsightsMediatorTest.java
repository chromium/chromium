// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.page_insights.PageInsightsMediator.PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.AutoPeekConditions;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsActionsHandler;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceRenderer;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceScope;
import org.chromium.chrome.browser.xsurface_provider.XSurfaceProcessScopeProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Unit tests for {@link PageInsightsMediator}.
 */
@LooperMode(Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public class PageInsightsMediatorTest {
    private static final String TEST_CHILD_PAGE_TITLE = "People also View";
    private static final byte[] TEST_FEED_ELEMENTS_OUTPUT = new byte[123];
    private static final byte[] TEST_CHILD_ELEMENTS_OUTPUT = new byte[456];

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    private LayoutInflater mLayoutInflater;
    @Mock
    private ObservableSupplier<Tab> mMockTabProvider;
    @Mock
    private ManagedBottomSheetController mBottomSheetController;
    @Mock
    private BottomSheetController mBottomUiController;
    @Mock
    private ExpandedSheetHelper mExpandedSheetHelper;
    @Mock
    private BrowserControlsStateProvider mControlsStateProvider;
    @Mock
    private BrowserControlsSizer mBrowserControlsSizer;
    @Mock
    private Tab mTab;
    @Mock
    private PageInsightsDataLoader mPageInsightsDataLoader;
    @Mock
    private ProcessScope mProcessScope;
    @Mock
    private PageInsightsSurfaceScope mSurfaceScope;
    @Mock
    private PageInsightsSurfaceRenderer mSurfaceRenderer;
    @Mock
    private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock
    private ShareDelegate mShareDelegate;
    @Mock
    private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderObserver;
    @Captor
    private ArgumentCaptor<Map<String, Object>> mSurfaceRendererContextValues;
    @Captor
    private ArgumentCaptor<LoadUrlParams> mLoadUrlParams;
    @Captor
    private ArgumentCaptor<ShareParams> mShareParams;

    private ShadowLooper mShadowLooper;

    private PageInsightsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context mContext = ContextUtils.getApplicationContext();
        mShadowLooper = ShadowLooper.shadowMainLooper();
        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);
        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(any(String.class)))
                .thenAnswer((invocation) -> {
                    return new GURL((String) invocation.getArguments()[0]);
                });
        XSurfaceProcessScopeProvider.setProcessScopeForTesting(mProcessScope);
        when(mProcessScope.obtainPageInsightsSurfaceScope(
                     any(PageInsightsSurfaceScopeDependencyProviderImpl.class)))
                .thenReturn(mSurfaceScope);
        when(mSurfaceScope.provideSurfaceRenderer()).thenReturn(mSurfaceRenderer);
        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        when(mPageInsightsDataLoader.getData()).thenReturn(getPageInsightsMetadata());
        when(mMockTabProvider.get()).thenReturn(mTab);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        mMediator = new PageInsightsMediator(mContext, mMockTabProvider, mShareDelegateSupplier,
                mBottomSheetController, mBottomUiController, mExpandedSheetHelper,
                mControlsStateProvider, mBrowserControlsSizer, () -> true);
        mMediator.setPageInsightsDataLoaderForTesting(mPageInsightsDataLoader);
        verify(mControlsStateProvider).addObserver(mBrowserControlsStateProviderObserver.capture());
        setBackgroundDrawable();
    }

    @Test
    @MediumTest
    public void testAutoTrigger_doesNotTriggerImmediately() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END, "2000");
        FeatureList.setTestValues(testValues);

        mMediator.onLoadStopped(mTab, true);
        mBrowserControlsStateProviderObserver.getValue().onControlsOffsetChanged(0, 70, 0, 0, true);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_notEnoughDuration_doesNotTrigger() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END, "2000");
        FeatureList.setTestValues(testValues);

        mMediator.onLoadStopped(mTab, true);
        mShadowLooper.idleFor(250, TimeUnit.MILLISECONDS);
        mBrowserControlsStateProviderObserver.getValue().onControlsOffsetChanged(0, 70, 0, 0, true);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_enoughDuration_showsBottomSheet() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END, "2000");
        FeatureList.setTestValues(testValues);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);

        mMediator.onLoadStopped(mTab, true);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());

        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);
        mBrowserControlsStateProviderObserver.getValue().onControlsOffsetChanged(0, 70, 0, 0, true);

        verify(mBottomSheetController, times(1)).requestShowContent(any(), anyBoolean());
        assertEquals(View.VISIBLE,
                mMediator.getSheetContent()
                        .getToolbarView()
                        .findViewById(R.id.page_insights_feed_header)
                        .getVisibility());
        assertEquals(View.VISIBLE,
                mMediator.getSheetContent()
                        .getContentView()
                        .findViewById(R.id.page_insights_feed_content)
                        .getVisibility());
        assertEquals(feedView,
                ((FrameLayout) mMediator.getSheetContent().getContentView().findViewById(
                         R.id.page_insights_feed_content))
                        .getChildAt(0));
        verify(mBottomSheetController, never()).expandSheet();
    }

    @Test
    @MediumTest
    public void testOpenInExpandedState_showsBottomSheet() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        FeatureList.setTestValues(testValues);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);

        mMediator.openInExpandedState();

        verify(mBottomSheetController, times(1)).requestShowContent(any(), anyBoolean());
        assertEquals(View.VISIBLE,
                mMediator.getSheetContent()
                        .getToolbarView()
                        .findViewById(R.id.page_insights_feed_header)
                        .getVisibility());
        assertEquals(View.VISIBLE,
                mMediator.getSheetContent()
                        .getContentView()
                        .findViewById(R.id.page_insights_feed_content)
                        .getVisibility());
        assertEquals(feedView,
                ((FrameLayout) mMediator.getSheetContent().getContentView().findViewById(
                         R.id.page_insights_feed_content))
                        .getChildAt(0));
        verify(mBottomSheetController).expandSheet();
    }

    @Test
    @MediumTest
    public void actionHandler_navigateToPageInsightsPage_childPageOpened() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        FeatureList.setTestValues(testValues);
        View childView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        when(mSurfaceRenderer.render(eq(TEST_CHILD_ELEMENTS_OUTPUT), any())).thenReturn(childView);
        mMediator.openInExpandedState();

        ((PageInsightsActionsHandler) mSurfaceRendererContextValues.getValue().get(
                 PageInsightsActionsHandler.KEY))
                .navigateToPageInsightsPage(1);

        assertEquals(View.VISIBLE,
                mMediator.getSheetContent()
                        .getToolbarView()
                        .findViewById(R.id.page_insights_child_page_header)
                        .getVisibility());
        assertEquals(View.VISIBLE,
                mMediator.getSheetContent()
                        .getContentView()
                        .findViewById(R.id.page_insights_child_content)
                        .getVisibility());
        assertEquals(childView,
                ((FrameLayout) mMediator.getSheetContent().getContentView().findViewById(
                         R.id.page_insights_child_content))
                        .getChildAt(0));
        TextView childPageTitle = mMediator.getSheetContent().getToolbarView().findViewById(
                R.id.page_insights_child_title);
        assertEquals(childPageTitle.getText(), TEST_CHILD_PAGE_TITLE);
    }

    @Test
    @MediumTest
    public void actionHandler_openUrl_opensUrl() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        FeatureList.setTestValues(testValues);
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        mMediator.openInExpandedState();

        String url = "https://www.realwebsite.com/";
        ((PageInsightsActionsHandler) mSurfaceRendererContextValues.getValue().get(
                 PageInsightsActionsHandler.KEY))
                .openUrl(url, /* doesRequestSpecifySameSession= */ false);

        verify(mTab).loadUrl(mLoadUrlParams.capture());
        assertEquals(url, mLoadUrlParams.getValue().getUrl());
    }

    @Test
    @MediumTest
    public void actionHandler_share_shares() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        FeatureList.setTestValues(testValues);
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        mMediator.openInExpandedState();

        String url = "https://www.realwebsite.com/";
        String title = "Real Website TM";
        ((PageInsightsActionsHandler) mSurfaceRendererContextValues.getValue().get(
                 PageInsightsActionsHandler.KEY))
                .share(url, title);

        verify(mShareDelegate).share(mShareParams.capture(), any(), eq(ShareOrigin.PAGE_INSIGHTS));
        assertEquals(url, mShareParams.getValue().getUrl());
        assertEquals(title, mShareParams.getValue().getTitle());
    }

    private PageInsightsMetadata getPageInsightsMetadata() {
        Page childPage = Page.newBuilder()
                                 .setId(Page.PageID.PEOPLE_ALSO_VIEW)
                                 .setTitle(TEST_CHILD_PAGE_TITLE)
                                 .setElementsOutput(ByteString.copyFrom(TEST_CHILD_ELEMENTS_OUTPUT))
                                 .build();
        Page feedPage = Page.newBuilder()
                                .setId(Page.PageID.SINGLE_FEED_ROOT)
                                .setTitle("Related Insights")
                                .setElementsOutput(ByteString.copyFrom(TEST_FEED_ELEMENTS_OUTPUT))
                                .build();
        AutoPeekConditions mAutoPeekConditions = AutoPeekConditions.newBuilder()
                                                         .setConfidence(0.51f)
                                                         .setPageScrollFraction(0.4f)
                                                         .setMinimumSecondsOnPage(30)
                                                         .build();
        return PageInsightsMetadata.newBuilder()
                .setFeedPage(feedPage)
                .addPages(childPage)
                .setAutoPeekConditions(mAutoPeekConditions)
                .build();
    }

    private void setBackgroundDrawable() {
        // Making a ViewGroup only for testing purposes to provide with the value of Background
        // Drawable (mBackgroundDrawable; which is a Gradient Drawable in PageInsightsMediator)
        ViewGroup rootView = new LinearLayout(ContextUtils.getApplicationContext());
        View backgroundView = new View(ContextUtils.getApplicationContext());
        backgroundView.setId(R.id.background);
        rootView.addView(backgroundView);
        backgroundView.setBackground(new GradientDrawable());
        mMediator.initView(rootView);
    }
}
