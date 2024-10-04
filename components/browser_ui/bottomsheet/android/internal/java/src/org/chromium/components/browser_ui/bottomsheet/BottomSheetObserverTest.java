// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheetObserver}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class BottomSheetObserverTest {
    /** An observer used to record events that occur with respect to the bottom sheet. */
    public static class TestSheetObserver extends EmptyBottomSheetObserver {
        /** A {@link CallbackHelper} that can wait for the bottom sheet to be closed. */
        public final CallbackHelper mClosedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the bottom sheet to be opened. */
        public final CallbackHelper mOpenedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onOffsetChanged event. */
        public final CallbackHelper mOffsetChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onSheetContentChanged event. */
        public final CallbackHelper mContentChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to be in its full state. */
        public final CallbackHelper mFullCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to be hidden. */
        public final CallbackHelper mHiddenCallbackHelper = new CallbackHelper();

        /** The last value that the onOffsetChanged event sent. */
        private float mLastOffsetChangedValue;

        @Override
        public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
            mLastOffsetChangedValue = heightFraction;
            mOffsetChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetOpened(@StateChangeReason int reason) {
            mOpenedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            mClosedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetContentChanged(BottomSheetContent newContent) {
            mContentChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetStateChanged(int newState, int reason) {
            if (newState == BottomSheetController.SheetState.HIDDEN) {
                mHiddenCallbackHelper.notifyCalled();
            } else if (newState == BottomSheetController.SheetState.FULL) {
                mFullCallbackHelper.notifyCalled();
            }
        }

        /** @return The last value passed in to {@link #onSheetOffsetChanged(float)}. */
        public float getLastOffsetChangedValue() {
            return mLastOffsetChangedValue;
        }
    }

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private TestSheetObserver mObserver;
    private TestBottomSheetContent mSheetContent;
    private BottomSheetControllerImpl mBottomSheetController;
    private ScrimCoordinator mScrimCoordinator;
    private BottomSheetTestSupport mTestSupport;

    @BeforeClass
    public static void setUpSuite() {
        sTestRule.launchActivity(null);
        BottomSheetTestSupport.setSmallScreen(false);
    }

    @Before
    public void setUp() throws Exception {
        ViewGroup rootView = sTestRule.getActivity().findViewById(android.R.id.content);
        ThreadUtils.runOnUiThreadBlocking(() -> rootView.removeAllViews());

        mScrimCoordinator =
                new ScrimCoordinator(
                        sTestRule.getActivity(),
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {
                                // Intentional noop
                            }

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {
                                // Intentional noop
                            }
                        },
                        rootView,
                        Color.WHITE);

        mBottomSheetController =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Supplier<ScrimCoordinator> scrimSupplier = () -> mScrimCoordinator;
                            Callback<View> initializedCallback = (v) -> {};
                            return new BottomSheetControllerImpl(
                                    scrimSupplier,
                                    initializedCallback,
                                    sTestRule.getActivity().getWindow(),
                                    KeyboardVisibilityDelegate.getInstance(),
                                    () -> rootView,
                                    false,
                                    () -> 0,
                                    /* desktopWindowStateProvider= */ null);
                        });

        mTestSupport = new BottomSheetTestSupport(mBottomSheetController);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent =
                            new TestBottomSheetContent(
                                    sTestRule.getActivity(),
                                    BottomSheetContent.ContentPriority.HIGH,
                                    false);
                    mBottomSheetController.requestShowContent(mSheetContent, false);

                    mObserver = new TestSheetObserver();
                    mBottomSheetController.addObserver(mObserver);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.destroy();
                    mScrimCoordinator.destroy();
                });
    }

    /** Test that the onSheetClosed event is triggered if the sheet is closed without animation. */
    @Test
    @MediumTest
    public void testCloseEventCalled_noAnimation() throws TimeoutException, ExecutionException {
        runCloseEventTest(false, true);
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed without animation and
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testCloseEventCalled_noAnimationNoPeekState()
            throws TimeoutException, ExecutionException {
        runCloseEventTest(false, false);
    }

    /** Test that the onSheetClosed event is triggered if the sheet is closed with animation. */
    @Test
    @MediumTest
    public void testCloseEventCalled_withAnimation() throws TimeoutException, ExecutionException {
        runCloseEventTest(true, true);
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed with animation but
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testCloseEventCalled_withAnimationNoPeekState()
            throws TimeoutException, ExecutionException {
        runCloseEventTest(true, false);
    }

    /**
     * Run different versions of the onSheetClosed event test.
     *
     * @param animationEnabled Whether to run the test with animation.
     * @param peekStateEnabled Whether the sheet's content has a peek state.
     */
    private void runCloseEventTest(boolean animationEnabled, boolean peekStateEnabled)
            throws TimeoutException, ExecutionException {
        CallbackHelper hiddenHelper = mObserver.mHiddenCallbackHelper;
        int initialHideEvents = hiddenHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.FULL, false));

        mSheetContent.setPeekHeight(
                peekStateEnabled
                        ? BottomSheetContent.HeightMode.DEFAULT
                        : BottomSheetContent.HeightMode.DISABLED);

        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;

        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        int closedCallbackCount = closedCallbackHelper.getCallCount();

        int targetState =
                peekStateEnabled
                        ? BottomSheetController.SheetState.PEEK
                        : BottomSheetController.SheetState.HIDDEN;
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(targetState, animationEnabled));

        closedCallbackHelper.waitForCallback(closedCallbackCount, 1);

        if (targetState == BottomSheetController.SheetState.HIDDEN) {
            hiddenHelper.waitForCallback(initialHideEvents, 1);
            assertFalse(
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    mBottomSheetController
                                            .getBottomSheetBackPressHandler()
                                            .getHandleBackPressChangedSupplier()
                                            .get()));
        }

        assertEquals(initialOpenedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertEquals(
                "Close event should have only been called once.",
                closedCallbackCount + 1,
                closedCallbackHelper.getCallCount());
    }

    /** Test that the onSheetOpened event is triggered if the sheet is opened without animation. */
    @Test
    @MediumTest
    public void testOpenedEventCalled_noAnimation() throws TimeoutException, ExecutionException {
        runOpenEventTest(false, true);
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened without animation and
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testOpenedEventCalled_noAnimationNoPeekState()
            throws TimeoutException, ExecutionException {
        runOpenEventTest(false, false);
    }

    /** Test that the onSheetOpened event is triggered if the sheet is opened with animation. */
    @Test
    @MediumTest
    public void testOpenedEventCalled_withAnimation() throws TimeoutException, ExecutionException {
        runOpenEventTest(true, true);
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened with animation and
     * without a peek state.
     */
    @Test
    @MediumTest
    public void testOpenedEventCalled_withAnimationNoPeekState()
            throws TimeoutException, ExecutionException {
        runOpenEventTest(true, false);
    }

    /**
     * Run different versions of the onSheetOpened event test.
     *
     * @param animationEnabled Whether to run the test with animation.
     * @param peekStateEnabled Whether the sheet's content has a peek state.
     */
    private void runOpenEventTest(boolean animationEnabled, boolean peekStateEnabled)
            throws TimeoutException, ExecutionException {
        mSheetContent.setPeekHeight(
                peekStateEnabled
                        ? BottomSheetContent.HeightMode.DEFAULT
                        : BottomSheetContent.HeightMode.DISABLED);

        CallbackHelper fullCallbackHelper = mObserver.mFullCallbackHelper;
        int initialFullCount = fullCallbackHelper.getCallCount();
        CallbackHelper openedCallbackHelper = mObserver.mOpenedCallbackHelper;
        int openedCallbackCount = openedCallbackHelper.getCallCount();
        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;
        int initialClosedCount = closedCallbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(mTestSupport.getOpeningState(), false));

        assertNotEquals(
                "Sheet should not be hidden.",
                mBottomSheetController.getSheetState(),
                BottomSheetController.SheetState.HIDDEN);
        if (!peekStateEnabled) {
            assertNotEquals(
                    "Sheet should be above the peeking state when peek is disabled.",
                    mBottomSheetController.getSheetState(),
                    BottomSheetController.SheetState.PEEK);
        }

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mTestSupport.setSheetState(
                                BottomSheetController.SheetState.FULL, animationEnabled));

        openedCallbackHelper.waitForCallback(openedCallbackCount, 1);
        fullCallbackHelper.waitForCallback(initialFullCount, 1);

        assertEquals(
                "Open event should have only been called once.",
                openedCallbackCount + 1,
                openedCallbackHelper.getCallCount());

        assertEquals(initialClosedCount, closedCallbackHelper.getCallCount());

        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mBottomSheetController
                                        .getBottomSheetBackPressHandler()
                                        .getHandleBackPressChangedSupplier()
                                        .get()));
    }

    /** Test the onOffsetChanged event. */
    @Test
    @MediumTest
    public void testOffsetChangedEvent() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.FULL, false));
        CallbackHelper callbackHelper = mObserver.mOffsetChangedCallbackHelper;

        float hiddenHeight =
                mTestSupport.getHiddenRatio() * mBottomSheetController.getContainerHeight();
        float fullHeight =
                mTestSupport.getFullRatio() * mBottomSheetController.getContainerHeight();

        // The sheet's half state is not necessarily 50% of the way to the top.
        float midPeekFull = (hiddenHeight + fullHeight) / 2f;

        // When in the hidden state, the transition value should be 0.
        int callbackCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetOffsetFromBottom(hiddenHeight, StateChangeReason.NONE));
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // When in the full state, the transition value should be 1.
        callbackCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetOffsetFromBottom(fullHeight, StateChangeReason.NONE));
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // Halfway between peek and full should send 0.5.
        callbackCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetOffsetFromBottom(midPeekFull, StateChangeReason.NONE));
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);
    }

    @Test
    @MediumTest
    public void testWrapContentBehavior() throws TimeoutException {
        // We make sure the height of the wrapped content is smaller than sheetContainerHeight.
        int wrappedContentHeight = (int) mBottomSheetController.getContainerHeight() / 2;
        assertTrue(wrappedContentHeight > 0);

        // Show content that should be wrapped.
        CallbackHelper callbackHelper = mObserver.mContentChangedCallbackHelper;
        int callCount = callbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // We wrap the View in a FrameLayout as we need something to read the
                    // hard coded height in the layout params. There is no way to create a
                    // View with a specific height on its own as View::onMeasure will by
                    // default set its height/width to be the minimum height/width of its
                    // background (if any) or expand as much as it can.
                    final ViewGroup contentView = new FrameLayout(sTestRule.getActivity());
                    View child = new View(sTestRule.getActivity());
                    child.setLayoutParams(
                            new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT, wrappedContentHeight));
                    contentView.addView(child);

                    mTestSupport.showContent(
                            new TestBottomSheetContent(
                                    sTestRule.getActivity(),
                                    BottomSheetContent.ContentPriority.HIGH,
                                    false) {
                                @Override
                                public View getContentView() {
                                    return contentView;
                                }

                                @Override
                                public float getFullHeightRatio() {
                                    return HeightMode.WRAP_CONTENT;
                                }
                            });
                });
        callbackHelper.waitForCallback(callCount);

        // HALF state is forbidden when wrapping the content.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.HALF, false));
        assertEquals(BottomSheetController.SheetState.FULL, mBottomSheetController.getSheetState());

        // Check the offset.
        assertEquals(
                wrappedContentHeight, mBottomSheetController.getCurrentOffset(), MathUtils.EPSILON);
    }
}
