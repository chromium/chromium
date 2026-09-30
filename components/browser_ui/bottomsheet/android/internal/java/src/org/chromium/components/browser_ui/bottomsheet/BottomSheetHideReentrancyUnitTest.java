// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for a real {@link BottomSheetControllerImpl} and {@link BottomSheet} hiding one content and
 * then showing the next queued content, which happens re-entrantly while observers are being
 * notified of {@link SheetState#HIDDEN}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetHideReentrancyUnitTest {
    private static final String HIDDEN_A = "hidden(current=A)";
    private static final String HIDDEN_B = "hidden(current=B)";
    private static final String CONTENT_B = "content:B";
    private static final String CONTENT_NULL = "content:null";
    private static final String SCROLLING = "scrolling";
    private static final String SETTLED = "settled";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ScrimManager mScrimManager;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private InsetObserver mInsetObserver;

    private Activity mActivity;
    private BottomSheetControllerImpl mController;
    private FakeContent mContentA;
    private FakeContent mContentB;

    /** Minimal real content. */
    private static class FakeContent implements BottomSheetContent {
        private final String mName;
        private final View mView;
        int mPriority = ContentPriority.HIGH;
        int mPeekHeight = HeightMode.DISABLED;
        boolean mCoversBottomControls;
        final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier =
                ObservableSuppliers.createNonNull(false);
        int mDestroyCount;
        @Nullable Runnable mOnDestroy;

        FakeContent(Activity activity, String name) {
            mName = name;
            mView = new FrameLayout(activity);
            mView.setLayoutParams(
                    new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 100));
        }

        @Override
        public View getContentView() {
            return mView;
        }

        @Override
        public @Nullable View getToolbarView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            return 0;
        }

        @Override
        public void destroy() {
            mDestroyCount++;
            if (mOnDestroy != null) mOnDestroy.run();
        }

        @Override
        public int getPriority() {
            return mPriority;
        }

        @Override
        public int getPeekHeight() {
            return mPeekHeight;
        }

        @Override
        public boolean coversBottomControls() {
            return mCoversBottomControls;
        }

        @Override
        public NonNullObservableSupplier<Boolean> getBackPressStateChangedSupplier() {
            return mBackPressStateChangedSupplier;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public int getSheetHalfHeightAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public int getSheetFullHeightAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public int getSheetClosedAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public String toString() {
            return mName;
        }
    }

    /** Records the target state whenever the sheet settles in an open state. */
    private class SettledTargetStateRecorder implements BottomSheetObserver {
        final List<Integer> mTargetStates = new ArrayList<>();

        @Override
        public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
            if (newState == SheetState.HIDDEN || newState == SheetState.SCROLLING) return;
            mTargetStates.add(mController.getTargetSheetState());
        }
    }

    /**
     * Records the events seen by an observer that is registered after the controller's own
     * observer, like every feature's observer.
     */
    private class EventRecorder implements BottomSheetObserver {
        final List<String> mEvents = new ArrayList<>();
        final List<Integer> mSheetStatesWhenHidden = new ArrayList<>();
        final List<Integer> mDestroyCountsWhenHidden = new ArrayList<>();

        @Override
        public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
            mEvents.add("content:" + newContent);
        }

        @Override
        public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
            if (newState == SheetState.HIDDEN) {
                mSheetStatesWhenHidden.add(mController.getSheetState());
                mDestroyCountsWhenHidden.add(mContentA.mDestroyCount);
                mEvents.add("hidden(current=" + mController.getCurrentSheetContent() + ")");
            } else if (newState == SheetState.SCROLLING) {
                mEvents.add(SCROLLING);
            } else {
                mEvents.add(SETTLED);
            }
        }
    }

    /** Runs {@code action} the first time the sheet is hidden. */
    private static class OnFirstHidden implements BottomSheetObserver {
        private final Runnable mAction;
        private boolean mRan;

        OnFirstHidden(Runnable action) {
            mAction = action;
        }

        @Override
        public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
            if (newState != SheetState.HIDDEN || mRan) return;
            mRan = true;
            mAction.run();
        }
    }

    @Before
    public void setUp() {
        mActivity = buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        FrameLayout root = new FrameLayout(mActivity);
        mActivity.setContentView(root);
        when(mInsetObserver.getSupplierForKeyboardInset())
                .thenReturn(ObservableSuppliers.createNonNull(0));

        OneshotSupplierImpl<ScrimManager> scrimSupplier = new OneshotSupplierImpl<>();
        scrimSupplier.set(mScrimManager);
        OneshotSupplierImpl<ViewGroup> rootSupplier = new OneshotSupplierImpl<>();
        rootSupplier.set(root);
        mController =
                new BottomSheetControllerImpl(
                        scrimSupplier,
                        mActivity.getWindow(),
                        mKeyboardVisibilityDelegate,
                        rootSupplier,
                        /* alwaysFullWidth= */ false,
                        () -> 0,
                        /* desktopWindowStateManager= */ null,
                        mInsetObserver,
                        /* enableLargeFormFactorUi= */ false);
        mContentA = new FakeContent(mActivity, "A");
        mContentB = new FakeContent(mActivity, "B");
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    private BottomSheet getSheet() {
        return (BottomSheet) mController.getBottomSheetViewForTesting();
    }

    /** Shows {@code content} and ends its opening animation. */
    private void showAndSettle(BottomSheetContent content) {
        assertTrue(mController.requestShowContent(content, /* animate= */ true));
        mController.endAnimationsForTesting();
        assertEquals(content, mController.getCurrentSheetContent());
    }

    /** Shows {@link #mContentA}, then queues {@link #mContentB} behind it. */
    private void showContentAAndQueueContentB() {
        showAndSettle(mContentA);
        assertFalse(mController.requestShowContent(mContentB, /* animate= */ true));
    }

    private EventRecorder addEventRecorder() {
        EventRecorder recorder = new EventRecorder();
        mController.addObserver(recorder);
        return recorder;
    }

    /** Asserts that {@code content} is showing and settled in its opening state. */
    private void assertSettledOpen(BottomSheetContent content) {
        assertEquals(content, mController.getCurrentSheetContent());
        assertEquals(getSheet().getOpeningState(), mController.getSheetState());
        assertEquals(SheetState.NONE, mController.getTargetSheetState());
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testAnimatedHide_endAnimationsSettlesNextContent() {
        showContentAAndQueueContentB();
        SettledTargetStateRecorder recorder = new SettledTargetStateRecorder();
        mController.addObserver(recorder);

        mController.hideContent(mContentA, /* animate= */ true);
        assertTrue(mController.isSheetHiding());
        // Ending A's hide animation synchronously starts B's opening animation, which must be
        // ended as well.
        mController.endAnimationsForTesting();

        assertEquals(mContentB, mController.getCurrentSheetContent());
        int openingState = getSheet().getOpeningState();
        assertEquals(openingState, mController.getSheetState());
        assertEquals(SheetState.NONE, mController.getTargetSheetState());
        assertFalse(mController.isSheetHiding());
        assertEquals(
                (int) getSheet().getSheetHeightForState(openingState),
                mController.getCurrentOffset());
        // The end of A's hide animation didn't reset the target state of B's opening animation.
        assertEquals(List.of(openingState), recorder.mTargetStates);
        assertEquals(1, mContentA.mDestroyCount);
        assertEquals(0, mContentB.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testNonAnimatedHide_nextContentKeepsTargetState() {
        showContentAAndQueueContentB();

        mController.hideContent(mContentA, /* animate= */ false);

        // B is opening. Its target state must survive the end of A's non-animated hide.
        assertEquals(mContentB, mController.getCurrentSheetContent());
        assertEquals(SheetState.SCROLLING, mController.getSheetState());
        int openingState = getSheet().getOpeningState();
        assertEquals(openingState, mController.getTargetSheetState());
        assertFalse(mController.isSheetHiding());

        // Suppressing the sheet while B opens restores B's opening state afterwards.
        int token = mController.suppressSheet(StateChangeReason.NONE);
        assertEquals(SheetState.HIDDEN, mController.getSheetState());
        mController.unsuppressSheet(token);
        mController.endAnimationsForTesting();

        assertEquals(mContentB, mController.getCurrentSheetContent());
        assertEquals(openingState, mController.getSheetState());
        assertEquals(SheetState.NONE, mController.getTargetSheetState());
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testHideWithQueuedContent_observersSeeHiddenBeforeNextContent() {
        showContentAAndQueueContentB();
        EventRecorder recorder = addEventRecorder();
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BottomSheet.Closed", StateChangeReason.NONE)
                        .expectBooleanRecord("Android.BottomSheet.Shown", true)
                        .build();

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_B, SCROLLING, SETTLED), recorder.mEvents);
        assertEquals(List.of(SheetState.HIDDEN), recorder.mSheetStatesWhenHidden);
        // A is only destroyed once every observer has been notified.
        assertEquals(List.of(0), recorder.mDestroyCountsWhenHidden);
        assertSettledOpen(mContentB);
        assertEquals(1, mContentA.mDestroyCount);
        histograms.assertExpected();
    }

    @Test
    @DisableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testHideWithQueuedContent_DeferContentSwapDisabled_nextContentShownBeforeHidden() {
        showContentAAndQueueContentB();
        EventRecorder recorder = addEventRecorder();

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        // The controller's own observer shows B before later observers are notified of HIDDEN.
        assertEquals(List.of(SCROLLING, CONTENT_B, SCROLLING, HIDDEN_B, SETTLED), recorder.mEvents);
        // By then, the sheet is already opening B.
        assertEquals(List.of(SheetState.SCROLLING), recorder.mSheetStatesWhenHidden);
        assertSettledOpen(mContentB);
        assertEquals(1, mContentA.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testHideWithEmptyQueue_observersSeeHiddenBeforeContentCleared() {
        showAndSettle(mContentA);
        EventRecorder recorder = addEventRecorder();

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_NULL), recorder.mEvents);
        assertNull(mController.getCurrentSheetContent());
        assertEquals(SheetState.HIDDEN, mController.getSheetState());
        assertEquals(View.GONE, mController.getBottomSheetContainerForTesting().getVisibility());
        assertEquals(1, mContentA.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testNonAnimatedHide_observersSeeHiddenBeforeNextContent() {
        showContentAAndQueueContentB();
        EventRecorder recorder = addEventRecorder();

        mController.hideContent(mContentA, /* animate= */ false);

        assertEquals(List.of(HIDDEN_A, CONTENT_B, SCROLLING), recorder.mEvents);
        assertEquals(getSheet().getOpeningState(), mController.getTargetSheetState());
        mController.endAnimationsForTesting();
        assertSettledOpen(mContentB);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testSupersede_observersSeeHiddenBeforeNextContent() {
        // A peeks, so it can be superseded by higher priority content.
        mContentA.mPriority = ContentPriority.LOW;
        mContentA.mPeekHeight = 50;
        showAndSettle(mContentA);
        assertEquals(SheetState.PEEK, mController.getSheetState());
        EventRecorder recorder = addEventRecorder();

        assertTrue(mController.requestShowContent(mContentB, /* animate= */ true));
        mController.endAnimationsForTesting();

        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_B, SCROLLING, SETTLED), recorder.mEvents);
        assertSettledOpen(mContentB);
        // A was only pushed aside: it isn't destroyed and comes back once B is hidden.
        assertEquals(0, mContentA.mDestroyCount);
        mController.hideContent(mContentB, /* animate= */ true);
        mController.endAnimationsForTesting();
        assertEquals(mContentA, mController.getCurrentSheetContent());
        assertEquals(SheetState.PEEK, mController.getSheetState());
        assertEquals(0, mContentA.mDestroyCount);
        assertEquals(1, mContentB.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testSuppressWhileHiding_destroysHiddenContentAndShowsNextAfterUnsuppress() {
        showContentAAndQueueContentB();
        EventRecorder recorder = addEventRecorder();
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BottomSheet.Closed", StateChangeReason.NONE)
                        .build();

        mController.hideContent(mContentA, /* animate= */ true);
        // Suppressing ends the hide without animation while the hide request is in progress.
        int token = mController.suppressSheet(StateChangeReason.NONE);

        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_NULL), recorder.mEvents);
        assertNull(mController.getCurrentSheetContent());
        assertEquals(SheetState.HIDDEN, mController.getSheetState());
        assertEquals(1, mContentA.mDestroyCount);
        histograms.assertExpected();

        mController.unsuppressSheet(token);
        mController.endAnimationsForTesting();
        assertSettledOpen(mContentB);
        assertEquals(1, mContentA.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testSuppressFromHiddenObserver_nextContentShownAfterUnsuppress() {
        mContentA.mCoversBottomControls = true;
        showContentAAndQueueContentB();
        View container = mController.getBottomSheetContainerForTesting();
        assertEquals(1.0f, container.getZ(), 0.0f);
        int[] token = new int[1];
        mController.addObserver(
                new OnFirstHidden(
                        () -> {
                            token[0] = mController.suppressSheet(StateChangeReason.NONE);
                            // The sheet is suppressed, so B won't be shown right away.
                            assertFalse(
                                    mController.requestShowContent(mContentB, /* animate= */ true));
                        }));
        EventRecorder recorder = addEventRecorder();

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_NULL), recorder.mEvents);
        assertNull(mController.getCurrentSheetContent());
        assertEquals(SheetState.HIDDEN, mController.getSheetState());
        // A no longer covers the bottom controls once it is removed from the sheet.
        assertEquals(0.0f, container.getZ(), 0.0f);

        mController.unsuppressSheet(token[0]);
        mController.endAnimationsForTesting();
        assertSettledOpen(mContentB);
        assertEquals(1, mContentA.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testSuppressAndUnsuppressFromHiddenObserver_showsNextContent() {
        showContentAAndQueueContentB();
        mController.addObserver(
                new OnFirstHidden(
                        () ->
                                mController.unsuppressSheet(
                                        mController.suppressSheet(StateChangeReason.NONE))));
        EventRecorder recorder = addEventRecorder();

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_B, SCROLLING, SETTLED), recorder.mEvents);
        assertSettledOpen(mContentB);
        assertEquals(1, mContentA.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testRequestShowContentFromHiddenObserver_emptyQueue_showsRequestedContent() {
        // B has higher priority than A, so outside of HIDDEN it would push A aside and queue it.
        mContentA.mPriority = ContentPriority.LOW;
        showAndSettle(mContentA);
        boolean[] shown = new boolean[1];
        mController.addObserver(
                new OnFirstHidden(
                        () ->
                                shown[0] =
                                        mController.requestShowContent(
                                                mContentB, /* animate= */ true)));
        EventRecorder recorder = addEventRecorder();
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.BottomSheet.Shown", true)
                        .build();

        swipeHide();

        assertTrue(shown[0]);
        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_B, SCROLLING, SETTLED), recorder.mEvents);
        assertSettledOpen(mContentB);
        assertEquals(1, mContentA.mDestroyCount);
        histograms.assertExpected();

        // A was dismissed, so it doesn't come back once B is hidden.
        mController.hideContent(mContentB, /* animate= */ true);
        mController.endAnimationsForTesting();
        assertNull(mController.getCurrentSheetContent());
        assertEquals(1, mContentA.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testRequestShowContentFromHiddenObserver_lowerPriority_isQueued() {
        showContentAAndQueueContentB();
        FakeContent contentC = new FakeContent(mActivity, "C");
        contentC.mPriority = ContentPriority.LOW;
        boolean[] shown = new boolean[1];
        mController.addObserver(
                new OnFirstHidden(
                        () ->
                                shown[0] =
                                        mController.requestShowContent(
                                                contentC, /* animate= */ true)));

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        assertFalse(shown[0]);
        assertSettledOpen(mContentB);
        mController.hideContent(mContentB, /* animate= */ true);
        mController.endAnimationsForTesting();
        assertSettledOpen(contentC);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testHideContentFromHiddenObserver_doesNotDestroyTwice() {
        showContentAAndQueueContentB();
        // Like some features, A's own observer hides A when the sheet is hidden, and A removes it
        // in destroy(). It gets HIDDEN for A before A is destroyed.
        int[] hiddenCount = new int[1];
        BottomSheetObserver contentObserver =
                new BottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(
                            @SheetState int newState, @StateChangeReason int reason) {
                        if (newState != SheetState.HIDDEN
                                || mController.getCurrentSheetContent() != mContentA) {
                            return;
                        }
                        hiddenCount[0]++;
                        mController.hideContent(mContentA, /* animate= */ true);
                    }
                };
        mController.addObserver(contentObserver);
        mContentA.mOnDestroy = () -> mController.removeObserver(contentObserver);
        EventRecorder recorder = addEventRecorder();
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BottomSheet.Closed", StateChangeReason.SWIPE)
                        .expectBooleanRecord("Android.BottomSheet.Shown", true)
                        .build();

        // Unlike hideContent(), a swipe doesn't mark a hide request as in progress.
        swipeHide();

        // B isn't shown in the middle of the loop.
        assertEquals(1, hiddenCount[0]);
        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_B, SCROLLING, SETTLED), recorder.mEvents);
        assertSettledOpen(mContentB);
        assertEquals(1, mContentA.mDestroyCount);
        histograms.assertExpected();
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testExpandAndCollapseFromHiddenObserver_areIgnored() {
        showContentAAndQueueContentB();
        boolean[] collapsed = new boolean[1];
        mController.addObserver(
                new OnFirstHidden(
                        () -> {
                            mController.expandSheet();
                            collapsed[0] = mController.collapseSheet(/* animate= */ true);
                        }));
        EventRecorder recorder = addEventRecorder();

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        assertFalse(collapsed[0]);
        assertEquals(List.of(SCROLLING, HIDDEN_A, CONTENT_B, SCROLLING, SETTLED), recorder.mEvents);
        assertSettledOpen(mContentB);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testDestroyFromHiddenObserver_doesNotShowNextContent() {
        showContentAAndQueueContentB();
        mController.addObserver(new OnFirstHidden(mController::destroy));
        EventRecorder recorder = addEventRecorder();

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        // Destroying the controller destroys A, which was waiting for the swap.
        assertFalse(recorder.mEvents.contains(CONTENT_B));
        assertEquals(1, mContentA.mDestroyCount);
        assertEquals(0, mContentB.mDestroyCount);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testBackPressStateWhileSwapPending_ignoresHiddenContent() {
        mContentA.mBackPressStateChangedSupplier.set(true);
        mContentB.mBackPressStateChangedSupplier.set(true);
        showContentAAndQueueContentB();
        NonNullObservableSupplier<Boolean> backPressSupplier =
                mController.getBottomSheetBackPressHandler().getHandleBackPressChangedSupplier();
        assertTrue(backPressSupplier.get());
        List<Boolean> backPressStatesWhenHidden = new ArrayList<>();
        mController.addObserver(
                new OnFirstHidden(() -> backPressStatesWhenHidden.add(backPressSupplier.get())));

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        // A still handled back presses, but it was already hidden.
        assertEquals(List.of(false), backPressStatesWhenHidden);
        assertSettledOpen(mContentB);
        assertTrue(backPressSupplier.get());
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testReenterHiddenFromHiddenObserver_commitsOnceAfterOutermostHidden() {
        showContentAAndQueueContentB();
        mController.addObserver(
                new OnFirstHidden(
                        () -> {
                            getSheet().setSheetState(SheetState.FULL, /* animate= */ false);
                            getSheet().setSheetState(SheetState.HIDDEN, /* animate= */ false);
                        }));
        EventRecorder recorder = addEventRecorder();
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.BottomSheet.Closed", StateChangeReason.NONE, 1)
                        .expectBooleanRecord("Android.BottomSheet.Shown", true)
                        .build();

        mController.hideContent(mContentA, /* animate= */ true);
        mController.endAnimationsForTesting();

        // B is only shown once the outermost HIDDEN has reached every observer.
        assertEquals(
                List.of(SCROLLING, SETTLED, HIDDEN_A, HIDDEN_A, CONTENT_B, SCROLLING, SETTLED),
                recorder.mEvents);
        assertSettledOpen(mContentB);
        assertEquals(1, mContentA.mDestroyCount);
        histograms.assertExpected();
    }

    /** Hides the sheet like a swipe does, without {@link BottomSheetController#hideContent}. */
    private void swipeHide() {
        getSheet().setSheetState(SheetState.HIDDEN, /* animate= */ true, StateChangeReason.SWIPE);
        mController.endAnimationsForTesting();
    }
}
