// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
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
        int mDestroyCount;

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
        }

        @Override
        public int getPriority() {
            return ContentPriority.HIGH;
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
}
