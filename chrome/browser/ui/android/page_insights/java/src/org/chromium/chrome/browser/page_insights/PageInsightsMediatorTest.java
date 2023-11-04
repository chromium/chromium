// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.page_insights.PageInsightsMediator.DEFAULT_TRIGGER_DELAY_MS;
import static org.chromium.chrome.browser.page_insights.PageInsightsMediator.PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.text.format.DateUtils;
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
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.page_insights.PageInsightsMediator.PageInsightsEvent;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.AutoPeekConditions;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsActionsHandler;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsLoggingParameters;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceRenderer;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceScope;
import org.chromium.chrome.browser.xsurface_provider.XSurfaceProcessScopeProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.function.Function;

/**
 * Unit tests for {@link PageInsightsMediator}.
 */
@LooperMode(Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public class PageInsightsMediatorTest {
    private static final String TEST_CHILD_PAGE_TITLE = "People also View";
    private static final byte[] TEST_FEED_ELEMENTS_OUTPUT = new byte[123];
    private static final byte[] TEST_CHILD_ELEMENTS_OUTPUT = new byte[456];
    private static final byte[] TEST_LOGGING_CGI = new byte[789];
    private static final int SHORT_TRIGGER_DELAY_MS = 2 * (int) DateUtils.SECOND_IN_MILLIS;

    @Rule
    public JniMocker jniMocker = new JniMocker();
    @Rule
    public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock
    protected OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;
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
    private ProcessScope mProcessScope;
    @Mock
    private Supplier<Profile> mProfileSupplier;
    @Mock
    private Profile mProfile;
    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private CoreAccountInfo mCoreAccountInfo;
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
    @Mock
    private BackPressManager mBackPressManager;
    @Mock
    private BackPressHandler mBackPressHandler;
    @Mock
    private Function<NavigationHandle, PageInsightsConfig> mPageInsightsConfigProvider;
    @Mock
    private NavigationHandle mNavigationHandle;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderObserver;

    @Captor private ArgumentCaptor<Map<String, Object>> mSurfaceRendererContextValues;
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParams;
    @Captor private ArgumentCaptor<ShareParams> mShareParams;
    @Captor private ArgumentCaptor<Callback<Tab>> mTabObserver;
    @Captor private ArgumentCaptor<PageInsightsLoggingParameters> mLoggingParameters;

    private ShadowLooper mShadowLooper;

    private PageInsightsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mShadowLooper = ShadowLooper.shadowMainLooper();
        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);
        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(any(String.class)))
                .thenAnswer((invocation) -> {
                    return new GURL((String) invocation.getArguments()[0]);
                });
        jniMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();
        Profile.setLastUsedProfileForTesting(mProfile);
        XSurfaceProcessScopeProvider.setProcessScopeForTesting(mProcessScope);
        when(mProcessScope.obtainPageInsightsSurfaceScope(
                     any(PageInsightsSurfaceScopeDependencyProviderImpl.class)))
                .thenReturn(mSurfaceScope);
        when(mSurfaceScope.provideSurfaceRenderer()).thenReturn(mSurfaceRenderer);
        when(mMockTabProvider.get()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        when(mProfileSupplier.get()).thenReturn(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mBottomSheetController.getBottomSheetBackPressHandler()).thenReturn(mBackPressHandler);
        when(mPageInsightsConfigProvider.apply(any()))
                .thenReturn(
                        PageInsightsConfig.newBuilder()
                                .setShouldAutoTrigger(true)
                                .setShouldXsurfaceLog(true)
                                .setShouldAttachGaiaToRequest(true)
                                .build());
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
    }


    private void createMediator() {
        createMediator(DEFAULT_TRIGGER_DELAY_MS);
    }

    private void createMediator(int triggerDelayMs) {
        createMediator(triggerDelayMs, 0);
    }

    private void createMediator(int triggerDelayMs, long firstLoadTimeMs) {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END, String.valueOf(triggerDelayMs));
        FeatureList.setTestValues(testValues);
        Context context = ContextUtils.getApplicationContext();
        context.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);
        mMediator =
                new PageInsightsMediator(
                        context,
                        new View(ContextUtils.getApplicationContext()),
                        mMockTabProvider,
                        mShareDelegateSupplier,
                        mProfileSupplier,
                        mBottomSheetController,
                        mBottomUiController,
                        mExpandedSheetHelper,
                        mControlsStateProvider,
                        mBrowserControlsSizer,
                        mBackPressManager,
                        () -> true,
                        mPageInsightsConfigProvider);
        verify(mControlsStateProvider).addObserver(mBrowserControlsStateProviderObserver.capture());
        mockOptimizationGuideResponse(getPageInsightsMetadata());
        setBackgroundDrawable();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void
            constructor_backGestureRefactorEnabled_sameBackPressHandlerNotRegistered_registers() {
        createMediator();

        verify(mBackPressManager)
                .addHandler(mBackPressHandler, BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void
            constructor_backGestureRefactorEnabled_sameBackPressHandlerRegistered_doesNotRegister() {
        when(mBackPressManager.has(BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET))
                .thenReturn(true);

        createMediator();

        verify(mBackPressManager, never())
                .addHandler(mBackPressHandler, BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void constructor_backGestureRefactorDisabled_doesNotRegister() {
        createMediator();

        verify(mBackPressManager, never())
                .addHandler(mBackPressHandler, BackPressHandler.Type.PAGE_INSIGHTS_BOTTOM_SHEET);
    }

    @Test
    @MediumTest
    public void testAutoTrigger_doesNotTriggerImmediately() {
        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        createMediator(SHORT_TRIGGER_DELAY_MS);
        mMediator.onPageLoadStarted(mTab, null);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_notEnoughDuration_controlsOffScreen_doesNotTrigger() {
        createMediator(SHORT_TRIGGER_DELAY_MS);
        mMediator.onPageLoadStarted(mTab, null);
        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        mShadowLooper.idleFor(250, TimeUnit.MILLISECONDS);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    @MediumTest
    public void
            testAutoTrigger_enoughDuration_controlsOffScreen_shouldNotAutoTrigger_doesNotTrigger() {
        when(mPageInsightsConfigProvider.apply(mNavigationHandle))
                .thenReturn(PageInsightsConfig.newBuilder().setShouldAutoTrigger(false).build());
        createMediator(SHORT_TRIGGER_DELAY_MS);
        mMediator.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);
        mMediator.onPageLoadStarted(mTab, null);

        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verifyNoMoreInteractions(mOptimizationGuideBridgeJniMock);
    }

    @Test
    @MediumTest
    public void testAutoTrigger_enoughDuration_controlsOnScreen_a11yDisabled_doesNotTrigger() {
        createMediator(SHORT_TRIGGER_DELAY_MS);
        mMediator.onPageLoadStarted(mTab, null);

        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(.0f);
        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verifyNoMoreInteractions(mOptimizationGuideBridgeJniMock);
    }

    @Test
    @MediumTest
    public void testAutoTrigger_enoughDuration_controlsOnScreen_a11yEnabled_showsBottomSheet() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
        createMediator(SHORT_TRIGGER_DELAY_MS);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);

        mMediator.onPageLoadStarted(mTab, null);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());

        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(.0f);
        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);

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
        assertNotEquals(
                PageInsightsSheetContent.HeightMode.DISABLED,
                mMediator.getSheetContent().getPeekHeight());
        verify(mBottomSheetController, never()).expandSheet();
    }

    @Test
    @MediumTest
    public void testAutoTrigger_enoughDuration_controlsOffScreen_showsBottomSheet() {
        createMediator(SHORT_TRIGGER_DELAY_MS);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);

        mMediator.onPageLoadStarted(mTab, null);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());

        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);

        verify(mBottomSheetController, times(1)).requestShowContent(any(), anyBoolean());
        assertEquals(
                View.VISIBLE,
                mMediator
                        .getSheetContent()
                        .getToolbarView()
                        .findViewById(R.id.page_insights_feed_header)
                        .getVisibility());
        assertEquals(
                View.VISIBLE,
                mMediator
                        .getSheetContent()
                        .getContentView()
                        .findViewById(R.id.page_insights_feed_content)
                        .getVisibility());
        assertEquals(
                feedView,
                ((FrameLayout)
                                mMediator
                                        .getSheetContent()
                                        .getContentView()
                                        .findViewById(R.id.page_insights_feed_content))
                        .getChildAt(0));
        assertNotEquals(
                PageInsightsSheetContent.HeightMode.DISABLED,
                mMediator.getSheetContent().getPeekHeight());
        verify(mBottomSheetController, never()).expandSheet();
    }

    @Test
    @MediumTest
    public void
            testAutoTrigger_enoughDuration_controlsOffScreen_signedIn_providesBothXSurfaceLoggingParamsAndLogs()
                    throws Exception {
        createMediator(SHORT_TRIGGER_DELAY_MS);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(mCoreAccountInfo);
        when(mCoreAccountInfo.getEmail()).thenReturn("email@address.com");

        mMediator.onPageLoadStarted(mTab, null);
        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);

        verify(mSurfaceRenderer).onSurfaceCreated(mLoggingParameters.capture());
        assertEquals(
                ByteString.copyFrom(TEST_LOGGING_CGI),
                ByteString.copyFrom(mLoggingParameters.getValue().parentData()));
        assertEquals("email@address.com", mLoggingParameters.getValue().accountName());
        verify(mSurfaceRenderer)
                .onEvent(
                        org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent
                                .BOTTOM_SHEET_PEEKING);
    }

    @Test
    @MediumTest
    public void
            testAutoTrigger_enoughDuration_controlsOffScreen_signedOut_providesPartialXSurfaceLoggingParamsAndLogs()
                    throws Exception {
        createMediator(SHORT_TRIGGER_DELAY_MS);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);

        mMediator.onPageLoadStarted(mTab, null);
        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);

        verify(mSurfaceRenderer).onSurfaceCreated(mLoggingParameters.capture());
        assertEquals(
                ByteString.copyFrom(TEST_LOGGING_CGI),
                ByteString.copyFrom(mLoggingParameters.getValue().parentData()));
        assertNull(mLoggingParameters.getValue().accountName());
        verify(mSurfaceRenderer)
                .onEvent(
                        org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent
                                .BOTTOM_SHEET_PEEKING);
    }

    @Test
    @MediumTest
    public void testLaunch_shouldNotAttachGaia_doesNotAttachGaia() throws Exception {
        when(mPageInsightsConfigProvider.apply(mNavigationHandle))
                .thenReturn(
                        PageInsightsConfig.newBuilder()
                                .setShouldAttachGaiaToRequest(false)
                                .build());
        createMediator();
        mMediator.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);

        mMediator.launch();

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationOnDemand(
                        anyLong(),
                        any(),
                        any(),
                        eq(
                                CommonTypesProto.RequestContext
                                        .CONTEXT_NON_PERSONALIZED_PAGE_INSIGHTS_HUB
                                        .getNumber()),
                        any());
    }

    @Test
    @MediumTest
    public void testLaunch_shouldAttachGaia_attachesGaia() throws Exception {
        when(mPageInsightsConfigProvider.apply(mNavigationHandle))
                .thenReturn(
                        PageInsightsConfig.newBuilder().setShouldAttachGaiaToRequest(true).build());
        createMediator();
        mMediator.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);

        mMediator.launch();

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationOnDemand(
                        anyLong(),
                        any(),
                        any(),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB.getNumber()),
                        any());
    }

    @Test
    @MediumTest
    public void testLaunch_showsBottomSheet() throws Exception {
        createMediator();
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);

        mMediator.launch();

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
        assertEquals(
                PageInsightsSheetContent.HeightMode.DISABLED,
                mMediator.getSheetContent().getPeekHeight());
        verify(mBottomSheetController).expandSheet();
    }

    @Test
    @MediumTest
    public void testLaunch_signedIn_providesFullXSurfaceLoggingParamsAndLogs() throws Exception {
        createMediator();
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(mCoreAccountInfo);
        when(mCoreAccountInfo.getEmail()).thenReturn("email@address.com");

        mMediator.launch();

        verify(mSurfaceRenderer).onSurfaceCreated(mLoggingParameters.capture());
        assertEquals(
                ByteString.copyFrom(TEST_LOGGING_CGI),
                ByteString.copyFrom(mLoggingParameters.getValue().parentData()));
        assertEquals("email@address.com", mLoggingParameters.getValue().accountName());
        verify(mSurfaceRenderer)
                .onEvent(
                        org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent
                                .BOTTOM_SHEET_EXPANDED);
    }

    @Test
    @MediumTest
    public void testLaunch_signedIn_shouldNotXSurfaceLog_doesNotCallOnSurfaceCreated()
            throws Exception {
        when(mPageInsightsConfigProvider.apply(mNavigationHandle))
                .thenReturn(PageInsightsConfig.newBuilder().setShouldXsurfaceLog(false).build());
        createMediator();
        mMediator.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(mCoreAccountInfo);
        when(mCoreAccountInfo.getEmail()).thenReturn("email@address.com");

        mMediator.launch();

        verify(mSurfaceRenderer, never()).onSurfaceCreated(any());
    }

    @Test
    @MediumTest
    public void testLaunch_signedOut_providesPartialhXSurfaceLoggingParamsAndLogs()
            throws Exception {
        createMediator();
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);

        mMediator.launch();

        verify(mSurfaceRenderer).onSurfaceCreated(mLoggingParameters.capture());
        assertEquals(
                ByteString.copyFrom(TEST_LOGGING_CGI),
                ByteString.copyFrom(mLoggingParameters.getValue().parentData()));
        assertNull(mLoggingParameters.getValue().accountName());
        verify(mSurfaceRenderer)
                .onEvent(
                        org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent
                                .BOTTOM_SHEET_EXPANDED);
    }

    @Test
    @MediumTest
    public void actionHandler_navigateToPageInsightsPage_childPageOpened() {
        createMediator();
        View childView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        when(mSurfaceRenderer.render(eq(TEST_CHILD_ELEMENTS_OUTPUT), any())).thenReturn(childView);
        mMediator.launch();

        ((PageInsightsActionsHandler) mSurfaceRendererContextValues.getValue().get(
                 PageInsightsActionsHandler.KEY))
                .navigateToPageInsightsPage(1);

        assertEquals(View.VISIBLE,
                mMediator.getSheetContent()
                        .getToolbarView()
                        .findViewById(R.id.page_insights_child_title)
                        .getVisibility());
        assertEquals(
                View.VISIBLE,
                mMediator
                        .getSheetContent()
                        .getToolbarView()
                        .findViewById(R.id.page_insights_back_button)
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
    public void actionHandler_openUrl_opensUrl() {
        createMediator();
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        mMediator.launch();

        String url = "https://www.realwebsite.com/";
        ((PageInsightsActionsHandler) mSurfaceRendererContextValues.getValue().get(
                 PageInsightsActionsHandler.KEY))
                .openUrl(url, /* doesRequestSpecifySameSession= */ false);

        verify(mTab).loadUrl(mLoadUrlParams.capture());
        assertEquals(url, mLoadUrlParams.getValue().getUrl());
    }

    @Test
    @MediumTest
    public void actionHandler_share_shares() {
        createMediator();
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        mMediator.launch();

        String url = "https://www.realwebsite.com/";
        String title = "Real Website TM";
        ((PageInsightsActionsHandler) mSurfaceRendererContextValues.getValue().get(
                 PageInsightsActionsHandler.KEY))
                .share(url, title);

        verify(mShareDelegate).share(mShareParams.capture(), any(), eq(ShareOrigin.PAGE_INSIGHTS));
        assertEquals(url, mShareParams.getValue().getUrl());
        assertEquals(title, mShareParams.getValue().getTitle());
    }

    @Test
    @MediumTest
    public void openInExpandedState_recordsHistogram_userInvokesPih() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("CustomTabs.PageInsights.Event",
                        PageInsightsMediator.PageInsightsEvent.USER_INVOKES_PIH);
        createMediator();
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        mMediator.launch();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testAutoTrigger_recordsHistogram_autoPeekTriggered() {
        HistogramWatcher histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "CustomTabs.PageInsights.Event", PageInsightsEvent.AUTO_PEEK_TRIGGERED);

        createMediator(SHORT_TRIGGER_DELAY_MS);
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(eq(TEST_FEED_ELEMENTS_OUTPUT), any())).thenReturn(feedView);

        mMediator.onPageLoadStarted(mTab, null);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());

        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);

        verify(mBottomSheetController, times(1)).requestShowContent(any(), anyBoolean());
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void swipeToExpandedState_recordsHistogramInStateExpanded() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.PageInsights.Event", PageInsightsEvent.STATE_EXPANDED);
        createMediator();

        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void swipeToExpandedStateFromPeek_xSurfaceLogs() {
        createMediator();
        mMediator.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);

        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);

        verify(mSurfaceRenderer)
                .onEvent(
                        org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent
                                .BOTTOM_SHEET_EXPANDED);
    }

    @Test
    @MediumTest
    public void openInExpandedState_updateToPeekState_recordsHistogramInStatePeek() {
        HistogramWatcher histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "CustomTabs.PageInsights.Event", PageInsightsMediator.PageInsightsEvent.STATE_PEEK);

        createMediator(SHORT_TRIGGER_DELAY_MS);
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));

        // STATE_PEEK is recorded
        mMediator.onSheetStateChanged(SheetState.PEEK, StateChangeReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void dismiss_unbindsXSurfaceViewsAndCallsOnSurfaceClosed() {
        createMediator();
        View childView = new View(ContextUtils.getApplicationContext());
        View feedView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(
                        eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(feedView);
        when(mSurfaceRenderer.render(eq(TEST_CHILD_ELEMENTS_OUTPUT), any())).thenReturn(childView);
        mMediator.launch();
        ((PageInsightsActionsHandler)
                        mSurfaceRendererContextValues
                                .getValue()
                                .get(PageInsightsActionsHandler.KEY))
                .navigateToPageInsightsPage(1);

        mMediator.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);

        verify(mSurfaceRenderer).unbindView(feedView);
        verify(mSurfaceRenderer).unbindView(childView);
        verify(mSurfaceRenderer).onSurfaceClosed();
    }

    @Test
    @MediumTest
    public void dismissFromPeekState_XSurfaceLogs() {
        createMediator(SHORT_TRIGGER_DELAY_MS);
        when(mSurfaceRenderer.render(
                        eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        mMediator.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);

        mMediator.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);

        verify(mSurfaceRenderer)
                .onEvent(
                        org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsEvent
                                .DISMISSED_FROM_PEEKING_STATE);
    }

    @Test
    @MediumTest
    public void dismissFromPeekState_recordsHistogramInDismissPeek() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("CustomTabs.PageInsights.Event",
                                PageInsightsEvent.STATE_PEEK, PageInsightsEvent.DISMISS_PEEK)
                        .build();

        createMediator(SHORT_TRIGGER_DELAY_MS);
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));

        // STATE_PEEK is recorded
        mMediator.onSheetStateChanged(SheetState.PEEK, StateChangeReason.SWIPE);
        // DISMISS_PEEK is recorded
        mMediator.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void dismissFromExpandedState_recordsHistogramInDismissExpanded() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("CustomTabs.PageInsights.Event",
                                PageInsightsEvent.STATE_EXPANDED,
                                PageInsightsEvent.DISMISS_EXPANDED)
                        .build();

        createMediator(SHORT_TRIGGER_DELAY_MS);
        when(mSurfaceRenderer.render(
                     eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));

        // STATE_PEEK is recorded
        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);

        // DISMISS_PEEK is recorded
        mMediator.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void
            navigateToPageInsightsPage_childPageOpened_recordsHistogram_userInteractsWithChildPage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.PageInsights.Event",
                                PageInsightsEvent.USER_INVOKES_PIH,
                                PageInsightsEvent.TAP_XSURFACE_VIEW_CHILD_PAGE)
                        .build();

        createMediator();
        View childView = new View(ContextUtils.getApplicationContext());
        when(mSurfaceRenderer.render(
                        eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        when(mSurfaceRenderer.render(eq(TEST_CHILD_ELEMENTS_OUTPUT), any())).thenReturn(childView);
        mMediator.launch();

        ((PageInsightsActionsHandler)
                        mSurfaceRendererContextValues
                                .getValue()
                                .get(PageInsightsActionsHandler.KEY))
                .navigateToPageInsightsPage(1);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void
            navigateToPageInsightsPage_openUrl_recordsHistogram_userInteractsWithSurfaceElement() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.PageInsights.Event",
                                PageInsightsEvent.USER_INVOKES_PIH,
                                PageInsightsEvent.TAP_XSURFACE_VIEW_URL)
                        .build();

        createMediator();
        when(mSurfaceRenderer.render(
                        eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        mMediator.launch();

        String url = "https://www.realwebsite.com/";
        ((PageInsightsActionsHandler)
                        mSurfaceRendererContextValues
                                .getValue()
                                .get(PageInsightsActionsHandler.KEY))
                .openUrl(url, /* doesRequestSpecifySameSession= */ false);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void navigateToPageInsightsPage_share_recordsHistogram_userInteractsWithShare() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.PageInsights.Event",
                                PageInsightsEvent.USER_INVOKES_PIH,
                                PageInsightsEvent.TAP_XSURFACE_VIEW_SHARE)
                        .build();

        createMediator();
        when(mSurfaceRenderer.render(
                        eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        mMediator.launch();

        String url = "https://www.realwebsite.com/";
        String title = "Real Website TM";
        ((PageInsightsActionsHandler)
                        mSurfaceRendererContextValues
                                .getValue()
                                .get(PageInsightsActionsHandler.KEY))
                .share(url, title);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void tapOnBottomSheet_peekState_expands() {
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.PEEK);
        createMediator();

        mMediator.getSheetContent().getToolbarView().callOnClick();

        verify(mBottomSheetController).expandSheet();
    }

    @Test
    @MediumTest
    public void tapOnBottomSheet_fullState_doesNotExpand() {
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        createMediator();

        mMediator.getSheetContent().getToolbarView().callOnClick();

        verify(mBottomSheetController, never()).expandSheet();
    }

    @Test
    @MediumTest
    public void handleBackPress_peekState_doesNothing() {
        createMediator();
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.PEEK);
        mMediator.onSheetStateChanged(SheetState.PEEK, StateChangeReason.SWIPE);

        assertFalse(mMediator.getSheetContent().getBackPressStateChangedSupplier().get());

        boolean handled = mMediator.getSheetContent().handleBackPress();

        assertFalse(handled);
        verify(mBottomSheetController, never())
                .hideContent(eq(mMediator.getSheetContent()), anyBoolean());
    }

    @Test
    @MediumTest
    public void handleBackPress_hiddenState_doesNothing() {
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.HIDDEN);
        createMediator();
        mMediator.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);

        assertFalse(mMediator.getSheetContent().getBackPressStateChangedSupplier().get());

        boolean handled = mMediator.getSheetContent().handleBackPress();

        assertFalse(handled);
        verify(mBottomSheetController, never())
                .hideContent(eq(mMediator.getSheetContent()), anyBoolean());
    }

    @Test
    @MediumTest
    public void handleBackPress_fullState_notChildPage_canCollapseToPeek_collapses() {
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        when(mBottomSheetController.collapseSheet(true)).thenReturn(true);
        createMediator();
        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);

        assertTrue(mMediator.getSheetContent().getBackPressStateChangedSupplier().get());

        boolean handled = mMediator.getSheetContent().handleBackPress();

        assertTrue(handled);
        verify(mBottomSheetController).collapseSheet(true);
        verify(mBottomSheetController, never()).hideContent(any(), anyBoolean());
    }

    @Test
    @MediumTest
    public void handleBackPress_fullState_notChildPage_cannotCollapseToPeek_hidesContent() {
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        when(mBottomSheetController.collapseSheet(true)).thenReturn(false);
        createMediator();
        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);

        assertTrue(mMediator.getSheetContent().getBackPressStateChangedSupplier().get());

        boolean handled = mMediator.getSheetContent().handleBackPress();

        assertTrue(handled);
        verify(mBottomSheetController).hideContent(mMediator.getSheetContent(), true);
    }

    @Test
    @MediumTest
    public void handleBackPress_fullState_childPage_goesBackToFeedPage() {
        when(mSurfaceRenderer.render(
                        eq(TEST_FEED_ELEMENTS_OUTPUT), mSurfaceRendererContextValues.capture()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        when(mSurfaceRenderer.render(eq(TEST_CHILD_ELEMENTS_OUTPUT), any()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        createMediator();
        mMediator.launch();
        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);
        ((PageInsightsActionsHandler)
                        mSurfaceRendererContextValues
                                .getValue()
                                .get(PageInsightsActionsHandler.KEY))
                .navigateToPageInsightsPage(1);

        assertTrue(mMediator.getSheetContent().getBackPressStateChangedSupplier().get());

        boolean handled = mMediator.getSheetContent().handleBackPress();

        assertTrue(handled);
        assertEquals(
                View.VISIBLE,
                mMediator
                        .getSheetContent()
                        .getToolbarView()
                        .findViewById(R.id.page_insights_feed_header)
                        .getVisibility());
        verify(mBottomSheetController, never())
                .hideContent(eq(mMediator.getSheetContent()), anyBoolean());
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
                .setLoggingCgi(ByteString.copyFrom(TEST_LOGGING_CGI))
                .build();
    }

    private void mockOptimizationGuideResponse(PageInsightsMetadata metadata) {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                OptimizationGuideBridge.OnDemandOptimizationGuideCallback callback =
                                        (OptimizationGuideBridge.OnDemandOptimizationGuideCallback)
                                                invocation.getArguments()[4];
                                callback.onOnDemandOptimizationGuideDecision(
                                        JUnitTestGURLs.EXAMPLE_URL,
                                        org.chromium.components.optimization_guide.proto.HintsProto
                                                .OptimizationType.PAGE_INSIGHTS,
                                        OptimizationGuideDecision.TRUE,
                                        CommonTypesProto.Any.newBuilder()
                                                .setValue(
                                                        ByteString.copyFrom(metadata.toByteArray()))
                                                .build());
                                return null;
                            }
                        })
                .when(mOptimizationGuideBridgeJniMock)
                .canApplyOptimizationOnDemand(
                        eq(1L),
                        eq(new GURL[] {JUnitTestGURLs.EXAMPLE_URL}),
                        eq(
                                new int[] {
                                    org.chromium.components.optimization_guide.proto.HintsProto
                                            .OptimizationType.PAGE_INSIGHTS
                                            .getNumber()
                                }),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB.getNumber()),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class));
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
