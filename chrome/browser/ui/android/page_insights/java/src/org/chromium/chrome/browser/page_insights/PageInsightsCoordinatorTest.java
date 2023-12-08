// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.page_insights.proto.PageInsights;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.AutoPeekConditions;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.Page;
import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
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
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

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

    @Mock private OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;
    @Mock private ObservableSupplierImpl<Tab> mTabProvider;
    @Captor private ArgumentCaptor<EmptyTabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomUiObserverCaptor;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateObserverCaptor;

    @Mock private Tab mTab;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private BrowserControlsSizer mBrowserControlsSizer;
    @Mock private BottomSheetController mBottomUiController;
    @Mock private ExpandedSheetHelper mExpandedSheetHelper;
    @Mock private BooleanSupplier mIsPageInsightsHubEnabled;
    @Mock private ProcessScope mProcessScope;
    @Mock private Supplier<Profile> mProfileSupplier;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PageInsightsSurfaceScope mSurfaceScope;
    @Mock private PageInsightsSurfaceRenderer mSurfaceRenderer;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private BackPressManager mBackPressManager;
    @Mock private ObservableSupplierImpl<Boolean> mInMotionSupplier;
    @Mock private NavigationHandle mNavigationHandle;

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
        jniMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();
        Profile.setLastUsedProfileForTesting(mProfile);
        TestThreadUtils.runOnUiThreadBlocking(() -> rootView().removeAllViews());
        XSurfaceProcessScopeProvider.setProcessScopeForTesting(mProcessScope);
        doReturn(mSurfaceScope).when(mProcessScope).obtainPageInsightsSurfaceScope(any());
        doReturn(mSurfaceRenderer).when(mSurfaceScope).provideSurfaceRenderer();
        doReturn(new View(ContextUtils.getApplicationContext()))
                .when(mSurfaceRenderer)
                .render(any(), any());
        doReturn(false).when(mIsPageInsightsHubEnabled).getAsBoolean();
        doReturn(mProfile).when(mProfileSupplier).get();
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(mProfile);
        doReturn(false).when(mInMotionSupplier).get();
        mFeatureListValues = new FeatureList.TestValues();
        FeatureList.setTestValues(mFeatureListValues);
        mFeatureListValues.addFieldTrialParamOverride(
                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PageInsightsMediator.PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END,
                String.valueOf(2000));
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
        waitForAnimationToFinish();
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
        doReturn(true).when(mIsPageInsightsHubEnabled).getAsBoolean();
        doReturn(mTab).when(mTabProvider).get();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();
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
                                        mBottomUiController,
                                        mExpandedSheetHelper,
                                        mBrowserControlsStateProvider,
                                        mBrowserControlsSizer,
                                        mBackPressManager,
                                        mInMotionSupplier,
                                        mIsPageInsightsHubEnabled,
                                        (navigationHandle) ->
                                                PageInsightsConfig.newBuilder()
                                                        .setShouldAutoTrigger(true)
                                                        .setShouldXsurfaceLog(true)
                                                        .setShouldAttachGaiaToRequest(true)
                                                        .build()));
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        mockOptimizationGuideResponse(pageInsights());
        mTestSupport = new BottomSheetTestSupport(mPageInsightsController);
        waitForAnimationToFinish();
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

    private void waitForAnimationToFinish() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.endAllAnimations());
    }

    private void expandSheet() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mPageInsightsController::expandSheet);
        waitForAnimationToFinish();
    }

    private void hideSheet() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPageInsightsController.hideContent(
                                mPageInsightsController.getCurrentSheetContent(), true));
        waitForAnimationToFinish();
    }

    private void hideTopBar() throws Exception {
        doReturn(1.f).when(mBrowserControlsStateProvider).getBrowserControlHiddenRatio();
        verify(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateObserverCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBrowserControlsStateObserverCaptor
                                .getValue()
                                .onControlsOffsetChanged(0, 0, 0, 0, false));
        waitForAnimationToFinish();

        // Sheet might not have opened.
        if (mPageInsightsController.getSheetState() == SheetState.HIDDEN) return;

        // Assert that the sheet is translated all the way down below the container i.e. the bottom
        // of the screen, thus invisible.
        View container = mPageInsightsCoordinator.getContainerForTesting();
        assertEquals(container.getHeight(), container.getTranslationY(), 0.01f);
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
        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
        setAutoTriggerTimerFinished();

        hideTopBar(); // Signal for auto triggering the PIH in Peek state
        assertEquals(0.f, mPageInsightsCoordinator.getCornerRadiusForTesting(), ASSERTION_DELTA);

        expandSheet();
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

        hideTopBar(); // Signal for auto triggering the PIH in Peek state
        View view = mBottomSheetContainer.findViewById(R.id.background);
        mBackgroundDrawable = (GradientDrawable) view.getBackground();
        assertEquals(
                sTestRule.getActivity().getColor(R.color.gm3_baseline_surface_container),
                mBackgroundDrawable.getColor().getDefaultColor());

        expandSheet();
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
    public void testResizeContent() throws Exception {
        createPageInsightsCoordinator();
        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
        setAutoTriggerTimerFinished();

        hideTopBar(); // Signal for auto triggering the PIH
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
    public void testHideWhenOtherBottomUiOpens() throws Exception {
        createAndLaunchPageInsightsCoordinator();

        // Invoke |onBottomUiStateChanged| directly - Contextual search
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPageInsightsCoordinator.onBottomUiStateChanged(true));
        waitForAnimationToFinish();
        assertEquals(
                "Sheet should be hidden",
                SheetState.HIDDEN,
                mPageInsightsController.getSheetState());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPageInsightsCoordinator.onBottomUiStateChanged(false));
        waitForAnimationToFinish();
        assertEquals(
                "Sheet should be restored",
                SheetState.FULL,
                mPageInsightsController.getSheetState());

        // Other bottom sheets
        verify(mBottomUiController).addObserver(mBottomUiObserverCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBottomUiObserverCaptor
                                .getValue()
                                .onSheetStateChanged(SheetState.PEEK, /* unused= */ 0));
        waitForAnimationToFinish();
        assertEquals(
                "Sheet should be hidden",
                SheetState.HIDDEN,
                mPageInsightsController.getSheetState());

        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBottomUiObserverCaptor
                                .getValue()
                                .onSheetStateChanged(SheetState.HIDDEN, /* unused= */ 0));
        waitForAnimationToFinish();
        assertEquals(
                "Sheet should be restored",
                SheetState.FULL,
                mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testExpandSheetHelper() throws Exception {
        createAndLaunchPageInsightsCoordinator();
        expandSheet();
        verify(mExpandedSheetHelper).onSheetExpanded();
        hideSheet();
        verify(mExpandedSheetHelper).onSheetCollapsed();
    }

    @Test
    @MediumTest
    public void testAutoTrigger() throws Exception {
        createPageInsightsCoordinator();
        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
        setAutoTriggerTimerFinished();

        hideTopBar(); // Signal for auto triggering the PIH

        assertEquals(SheetState.PEEK, mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_notEnoughDuration() throws Exception {
        createPageInsightsCoordinator();
        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());

        hideTopBar(); // Signal for auto triggering the PIH

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

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_notEnabled() throws Exception {
        createPageInsightsCoordinator();
        doReturn(false).when(mIsPageInsightsHubEnabled).getAsBoolean();

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());

        setAutoTriggerTimerFinished();
        hideTopBar(); // Signal for auto triggering the PIH

        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
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
                                        Any.newBuilder()
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
                        eq(new int[] {HintsProto.OptimizationType.PAGE_INSIGHTS.getNumber()}),
                        eq(CommonTypesProto.RequestContext.CONTEXT_PAGE_INSIGHTS_HUB.getNumber()),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class));
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
