// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
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
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * This class tests the functionality of the {@link PageInsights Hub}
 * through the coordinator API and the mediator.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PageInsightsCoordinatorTest {
    private static final float ASSERTION_DELTA = 0.01f;
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ObservableSupplier<Tab> mTabProvider;
    @Captor
    private ArgumentCaptor<Callback<Tab>> mTabCallbackCaptor;
    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    private ArgumentCaptor<BottomSheetObserver> mBottomUiObserverCaptor;
    @Mock
    private Tab mTab;
    @Mock
    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock
    private BrowserControlsSizer mBrowserControlsSizer;
    @Mock
    private BottomSheetController mBottomUiController;
    @Mock
    private ExpandedSheetHelper mExpandedSheetHelper;

    private PageInsightsCoordinator mPageInsightsCoordinator;
    private ManagedBottomSheetController mPageInsightsController;
    private ScrimCoordinator mScrimCoordinator;
    private BottomSheetTestSupport mTestSupport;

    @BeforeClass
    public static void setupSuite() {
        sTestRule.launchActivity(null);
    }

    @Before
    public void setupTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> rootView().removeAllViews());
    }

    private static Activity getActivity() {
        return sTestRule.getActivity();
    }

    private static ViewGroup rootView() {
        return getActivity().findViewById(android.R.id.content);
    }

    private void createPageInsightsCoordinator() throws Exception {
        Activity activity = getActivity();
        mScrimCoordinator =
                new ScrimCoordinator(activity, new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {}

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {}
                }, rootView(), Color.WHITE);

        mPageInsightsController = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Supplier<ScrimCoordinator> scrimSupplier = () -> mScrimCoordinator;
            Callback<View> initializedCallback = (v) -> mPageInsightsCoordinator.initView(v);
            return BottomSheetControllerFactory.createFullWidthBottomSheetController(scrimSupplier,
                    initializedCallback, activity.getWindow(),
                    KeyboardVisibilityDelegate.getInstance(), () -> rootView());
        });
        mPageInsightsCoordinator = new PageInsightsCoordinator(activity, mTabProvider,
                mPageInsightsController, mBottomUiController, mExpandedSheetHelper,
                mBrowserControlsStateProvider, mBrowserControlsSizer);
        mTestSupport = new BottomSheetTestSupport(mPageInsightsController);
        TestThreadUtils.runOnUiThreadBlocking(mPageInsightsCoordinator::launch);
        waitForAnimationToFinish();
    }

    @After
    public void tearDown() {
        if (mPageInsightsController == null) return;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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

    private void collapseSheet() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mPageInsightsController.collapseSheet(true));
        waitForAnimationToFinish();
    }

    @Test
    @MediumTest
    public void testRoundTopCornerAtExpandedState() throws Exception {
        createPageInsightsCoordinator();
        assertEquals(0.f, mPageInsightsCoordinator.getCornerRadiusForTesting(), ASSERTION_DELTA);

        expandSheet();
        int maxCornerRadiusPx = sTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.bottom_sheet_corner_radius);
        assertEquals(maxCornerRadiusPx, mPageInsightsCoordinator.getCornerRadiusForTesting(),
                ASSERTION_DELTA);
    }

    @Test
    @MediumTest
    public void testHideOnPageLoad() throws Exception {
        createPageInsightsCoordinator();
        verify(mTabProvider).addObserver(mTabCallbackCaptor.capture());
        mTabCallbackCaptor.getValue().onResult(mTab);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabObserverCaptor.getValue().onPageLoadFinished(mTab, null));
        waitForAnimationToFinish();

        // The very first page load should be kept.
        assertEquals(SheetState.PEEK, mPageInsightsController.getSheetState());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabObserverCaptor.getValue().onPageLoadFinished(mTab, null));
        waitForAnimationToFinish();
        assertEquals(SheetState.HIDDEN, mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testResizeContent() throws Exception {
        createPageInsightsCoordinator();
        int peekHeight = mPageInsightsController.getCurrentOffset();
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(peekHeight), eq(0));

        // Simulate dragging the sheet down below the peeking state. This should resize i.e. expand
        // the content.
        mTestSupport.forceScrolling(peekHeight / 2, 1.f);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mTestSupport.setSheetOffsetFromBottom(
                                peekHeight / 2, StateChangeReason.SWIPE));
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(0), eq(0));
    }

    @Test
    @MediumTest
    public void testHideWhenOtherBottomUiOpens() throws Exception {
        createPageInsightsCoordinator();

        // Invoke |onBottomUiStateChanged| directly - Contextual search
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPageInsightsCoordinator.onBottomUiStateChanged(true));
        waitForAnimationToFinish();
        assertEquals("Sheet should be hidden", SheetState.HIDDEN,
                mPageInsightsController.getSheetState());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPageInsightsCoordinator.onBottomUiStateChanged(false));
        waitForAnimationToFinish();
        assertEquals("Sheet should be restored", SheetState.PEEK,
                mPageInsightsController.getSheetState());

        // Other bottom sheets
        verify(mBottomUiController).addObserver(mBottomUiObserverCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mBottomUiObserverCaptor.getValue().onSheetStateChanged(
                                SheetState.PEEK, /*unused*/ 0));
        waitForAnimationToFinish();
        assertEquals("Sheet should be hidden", SheetState.HIDDEN,
                mPageInsightsController.getSheetState());

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mBottomUiObserverCaptor.getValue().onSheetStateChanged(
                                SheetState.HIDDEN, /*unused*/ 0));
        waitForAnimationToFinish();
        assertEquals("Sheet should be restored", SheetState.PEEK,
                mPageInsightsController.getSheetState());
    }

    @Test
    @MediumTest
    public void testExpandSheetHelper() throws Exception {
        createPageInsightsCoordinator();
        expandSheet();
        verify(mExpandedSheetHelper).onSheetExpanded();
        collapseSheet();
        verify(mExpandedSheetHelper).onSheetCollapsed();
    }
}
