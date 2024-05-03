// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.google.protobuf.ByteString;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactoryJni;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.page_insights.proto.IntentParams.PageInsightsIntentParams;
import org.chromium.chrome.browser.page_insights.proto.PageInsights;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.AutoPeekConditions;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceRenderer;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsSurfaceScope;
import org.chromium.chrome.browser.xsurface_provider.XSurfaceProcessScopeProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.function.BooleanSupplier;

/**
 * This class tests the functionality of the {@link PageInsights Hub} through the coordinator API
 * and the mediator.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PageInsightsCoordinatorTest {
    private static final float ASSERTION_DELTA = 1.01f;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private OptimizationGuideBridgeFactory.Natives mOptimizationGuideBridgeFactoryJniMock;
    @Mock private OptimizationGuideBridge mOptimizationGuideBridge;

    @Mock private ObservableSupplierImpl<Tab> mTabProvider;
    @Captor private ArgumentCaptor<EmptyTabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<BottomSheetObserver> mOtherBottomSheetObserverCaptor;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateObserverCaptor;

    @Mock private Tab mTab;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private BrowserControlsSizer mBrowserControlsSizer;
    @Mock private BottomSheetController mOtherBottomSheetController;
    @Mock private ExpandedSheetHelper mExpandedSheetHelper;
    @Mock private BooleanSupplier mIsPageInsightsHubEnabled;
    @Mock private ProcessScope mProcessScope;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PageInsightsSurfaceScope mSurfaceScope;
    @Mock private PageInsightsSurfaceRenderer mSurfaceRenderer;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private BackPressManager mBackPressManager;
    @Mock private ObservableSupplierImpl<Boolean> mInMotionSupplier;
    @Mock private NavigationHandle mNavigationHandle;
    @Mock private ApplicationViewportInsetSupplier mAppInsetSupplier;

    private Supplier<Profile> mProfileSupplier;
    private PageInsightsCoordinator mPageInsightsCoordinator;
    private ManagedBottomSheetController mPageInsightsController;
    private ScrimCoordinator mScrimCoordinator;
    private BottomSheetTestSupport mTestSupport;

    private View mBottomSheetContainer;

    private GradientDrawable mBackgroundDrawable;

    private FeatureList.TestValues mFeatureListValues;

    @BeforeClass
    public static void setupSuite() {
        sTestRule.launchActivity(null);
    }

    @Before
    public void setupTest() throws Exception {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(
                OptimizationGuideBridgeFactoryJni.TEST_HOOKS,
                mOptimizationGuideBridgeFactoryJniMock);
        when(mOptimizationGuideBridgeFactoryJniMock.getForProfile(mProfile))
                .thenReturn(mOptimizationGuideBridge);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        TestThreadUtils.runOnUiThreadBlocking(() -> rootView().removeAllViews());
        XSurfaceProcessScopeProvider.setProcessScopeForTesting(mProcessScope);
        when(mProcessScope.obtainPageInsightsSurfaceScope(any())).thenReturn(mSurfaceScope);
        when(mSurfaceScope.provideSurfaceRenderer()).thenReturn(mSurfaceRenderer);
        when(mSurfaceRenderer.render(any(), any()))
                .thenReturn(new View(ContextUtils.getApplicationContext()));
        when(mIsPageInsightsHubEnabled.getAsBoolean()).thenReturn(false);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mInMotionSupplier.get()).thenReturn(false);
        mFeatureListValues = new FeatureList.TestValues();
        FeatureList.setTestValues(mFeatureListValues);
        mFeatureListValues.addFieldTrialParamOverride(
                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PageInsightsMediator.PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END,
                String.valueOf(2000));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileSupplier = new ObservableSupplierImpl<>();
                    ((ObservableSupplierImpl) mProfileSupplier).set(mProfile);
                });
    }

    private static Activity getActivity() {
        return sTestRule.getActivity();
    }

    private static ViewGroup rootView() {
        return getActivity().findViewById(android.R.id.content);
    }

    private void createAndLaunchPageInsightsCoordinator() throws Exception {
        createPageInsightsCoordinator();
        TestThreadUtils.runOnUiThreadBlocking(mPageInsightsCoordinator::launch);
        waitForSheetState(SheetState.FULL);
    }

    private void createPageInsightsCoordinator() throws Exception {
        Activity activity = getActivity();
        mScrimCoordinator =
                new ScrimCoordinator(
                        activity,
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {}

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {}
                        },
                        rootView(),
                        Color.WHITE);

        mPageInsightsController =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Supplier<ScrimCoordinator> scrimSupplier = () -> mScrimCoordinator;
                            Callback<View> initializedCallback =
                                    (v) -> {
                                        mPageInsightsCoordinator.initView(v);
                                        mBottomSheetContainer = v;
                                    };
                            return BottomSheetControllerFactory
                                    .createFullWidthBottomSheetController(
                                            scrimSupplier,
                                            initializedCallback,
                                            activity.getWindow(),
                                            KeyboardVisibilityDelegate.getInstance(),
                                            () -> rootView());
                        });
        when(mIsPageInsightsHubEnabled.getAsBoolean()).thenReturn(true);
        when(mTabProvider.get()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        mPageInsightsCoordinator =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new PageInsightsCoordinator(
                                        activity,
                                        new View(ContextUtils.getApplicationContext()),
                                        mTabProvider,
                                        mShareDelegateSupplier,
                                        mProfileSupplier,
                                        mPageInsightsController,
                                        mOtherBottomSheetController,
                                        mExpandedSheetHelper,
                                        mBrowserControlsStateProvider,
                                        mBrowserControlsSizer,
                                        mBackPressManager,
                                        mInMotionSupplier,
                                        mAppInsetSupplier,
                                        PageInsightsIntentParams.getDefaultInstance(),
                                        mIsPageInsightsHubEnabled,
                                        (request) ->
                                                PageInsightsConfig.newBuilder()
                                                        .setIsInitialPage(true)
                                                        .setShouldAutoTrigger(true)
                                                        .setShouldXsurfaceLog(true)
                                                        .setServerShouldNotLogOrPersonalize(false)
                                                        .setNavigationTimestampMs(1234L)
                                                        .build()));
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        mockOptimizationGuideResponse(pageInsights());
        mTestSupport = new BottomSheetTestSupport(mPageInsightsController);
        endAnimations();
    }

    @After
    public void tearDown() {
        if (mPageInsightsController == null) return;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mScrimCoordinator.destroy();
                    mPageInsightsController.destroy();
                });
    }

    private void endAnimations() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.endAllAnimations());
    }

    private void expandSheet() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mPageInsightsController::expandSheet);
    }

    private void hideSheet() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPageInsightsController.hideContent(
                                mPageInsightsController.getCurrentSheetContent(), true));
    }

    private void hideTopBar() throws Exception {
        when(mBrowserControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.f);
        verify(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateObserverCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBrowserControlsStateObserverCaptor
                                .getValue()
                                .onControlsOffsetChanged(0, 0, 0, 0, false));
    }

    private void waitForSheetState(@SheetState int state) throws Exception {
        endAnimations();
        BottomSheetTestSupport.waitForState(mPageInsightsController, state);
    }

    private void setConfidenceTooLowForAutoTrigger() {
        mockOptimizationGuideResponse(pageInsights(0.4f));
    }

    private void setAutoTriggerTimerFinished() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPageInsightsCoordinator.onAutoTriggerTimerFinishedForTesting());
    }

    @Test
    @MediumTest
    public void testRoundTopCornerAtExpandedStateAfterPeekState() throws Exception {
        createPageInsightsCoordinator();

        setAutoTriggerTimerFinished();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        hideTopBar(); // Signal for auto triggering the PIH
        waitForSheetState(SheetState.PEEK);

        assertEquals(0.f, mPageInsightsCoordinator.getCornerRadiusForTesting(), ASSERTION_DELTA);

        expandSheet();
        waitForSheetState(SheetState.FULL);

        int maxCornerRadiusPx =
                sTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_corner_radius);
        assertEquals(
                maxCornerRadiusPx,
                mPageInsightsCoordinator.getCornerRadiusForTesting(),
                ASSERTION_DELTA);
    }

    @Test
    @MediumTest
    public void testRoundTopCornerAtFirstExpandedState() throws Exception {
        createAndLaunchPageInsightsCoordinator();

        int maxCornerRadiusPx =
                sTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_corner_radius);
        assertEquals(
                maxCornerRadiusPx,
                mPageInsightsCoordinator.getCornerRadiusForTesting(),
                ASSERTION_DELTA);
    }

    @Test
    @MediumTest
    public void testBackgroundColorAtExpandedStateAfterPeekState() throws Exception {
        createPageInsightsCoordinator();
        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());

        setAutoTriggerTimerFinished();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        hideTopBar(); // Signal for auto triggering the PIH in Peek state
        waitForSheetState(SheetState.PEEK);

        View view = mBottomSheetContainer.findViewById(R.id.background);
        mBackgroundDrawable = (GradientDrawable) view.getBackground();

        assertEquals(
                sTestRule.getActivity().getColor(R.color.gm3_baseline_surface_container),
                mBackgroundDrawable.getColor().getDefaultColor());

        expandSheet();
        waitForSheetState(SheetState.FULL);

        assertEquals(
                sTestRule.getActivity().getColor(R.color.gm3_baseline_surface),
                mBackgroundDrawable.getColor().getDefaultColor());
    }

    @Test
    @MediumTest
    public void testBackgroundColorAtFirstExpandedState() throws Exception {
        createAndLaunchPageInsightsCoordinator();

        View view = mBottomSheetContainer.findViewById(R.id.background);
        mBackgroundDrawable = (GradientDrawable) view.getBackground();

        assertEquals(
                sTestRule.getActivity().getColor(R.color.gm3_baseline_surface),
                mBackgroundDrawable.getColor().getDefaultColor());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/328462350")
    public void testResizeContent() throws Exception {
        createPageInsightsCoordinator();

        setAutoTriggerTimerFinished();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        hideTopBar(); // Signal for auto triggering the PIH
        waitForSheetState(SheetState.PEEK);

        int peekHeight = mPageInsightsController.getCurrentOffset();
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(peekHeight), eq(0));

        // Simulate dragging the sheet down below the peeking state. This should resize i.e. expand
        // the content.
        mTestSupport.forceScrolling(peekHeight / 2, 1.f);
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mTestSupport.setSheetOffsetFromBottom(
                                peekHeight / 2, StateChangeReason.SWIPE));

        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(0), eq(0));
    }

    @Test
    @MediumTest
    public void testOnBottomUiStateChanged_true_hides() throws Exception {
        createAndLaunchPageInsightsCoordinator();

        // Invoke |onBottomUiStateChanged| directly - Contextual search
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPageInsightsCoordinator.onBottomUiStateChanged(true));
        waitForSheetState(SheetState.HIDDEN);

        assertEquals(
                "Sheet should be hidden",
                SheetState.HIDDEN,
                mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testOtherBottomSheetObserverOnSheetChanged_peek_hides() throws Exception {
        createAndLaunchPageInsightsCoordinator();
        verify(mOtherBottomSheetController).addObserver(mOtherBottomSheetObserverCaptor.capture());

        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mOtherBottomSheetObserverCaptor
                                .getValue()
                                .onSheetStateChanged(SheetState.PEEK, /* unused= */ 0));
        waitForSheetState(SheetState.HIDDEN);

        assertEquals(
                "Sheet should be hidden",
                SheetState.HIDDEN,
                mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testExpandSheetHelper() throws Exception {
        createAndLaunchPageInsightsCoordinator();
        expandSheet();
        waitForSheetState(SheetState.FULL);

        verify(mExpandedSheetHelper).onSheetExpanded();

        hideSheet();
        waitForSheetState(SheetState.HIDDEN);

        verify(mExpandedSheetHelper).onSheetCollapsed();
    }

    @Test
    @MediumTest
    public void testAutoTrigger() throws Exception {
        createPageInsightsCoordinator();

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());

        setAutoTriggerTimerFinished();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        hideTopBar(); // Signal for auto triggering the PIH
        waitForSheetState(SheetState.PEEK);

        assertEquals(SheetState.PEEK, mPageInsightsController.getSheetState());
        // Assert that the sheet is translated all the way down below the container i.e. the
        // bottom of the screen, thus invisible.
        View container = mPageInsightsCoordinator.getContainerForTesting();
        assertEquals(container.getHeight(), container.getTranslationY(), 0.01f);
    }

    @Test
    @MediumTest
    public void testAutoTrigger_notEnoughDuration() throws Exception {
        createPageInsightsCoordinator();
        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());

        hideTopBar(); // Signal for auto triggering the PIH
        endAnimations();

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_notEnoughConfidence() throws Exception {
        createPageInsightsCoordinator();
        setConfidenceTooLowForAutoTrigger(); // By default, the confidence is over the threshold

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());

        setAutoTriggerTimerFinished();
        hideTopBar(); // Signal for auto triggering the PIH
        endAnimations();

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_notEnabled() throws Exception {
        createPageInsightsCoordinator();
        when(mIsPageInsightsHubEnabled.getAsBoolean()).thenReturn(false);

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());

        setAutoTriggerTimerFinished();
        hideTopBar(); // Signal for auto triggering the PIH
        endAnimations();

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
    }

    private void mockOptimizationGuideResponse(PageInsightsMetadata metadata) {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                OptimizationGuideBridge.OnDemandOptimizationGuideCallback callback =
                                        (OptimizationGuideBridge.OnDemandOptimizationGuideCallback)
                                                invocation.getArguments()[3];
                                callback.onOnDemandOptimizationGuideDecision(
                                        JUnitTestGURLs.EXAMPLE_URL,
                                        org.chromium.components.optimization_guide.proto.HintsProto
                                                .OptimizationType.PAGE_INSIGHTS,
                                        OptimizationGuideDecision.TRUE,
                                        Any.newBuilder()
                                                .setValue(
                                                        ByteString.copyFrom(metadata.toByteArray()))
                                                .build());
                                return null;
                            }
                        })
                .when(mOptimizationGuideBridge)
                .canApplyOptimizationOnDemand(
                        eq(Arrays.asList(JUnitTestGURLs.EXAMPLE_URL)),
                        eq(Arrays.asList(HintsProto.OptimizationType.PAGE_INSIGHTS)),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class),
                        any());
    }

    private static PageInsightsMetadata pageInsights() {
        return pageInsights(0.51f);
    }

    private static PageInsightsMetadata pageInsights(float confidence) {
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
                        .setConfidence(confidence)
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
