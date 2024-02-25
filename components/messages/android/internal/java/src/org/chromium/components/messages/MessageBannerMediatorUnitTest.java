// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static android.os.Looper.getMainLooper;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;
import static org.robolectric.annotation.LooperMode.Mode.PAUSED;

import android.animation.Animator;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.MotionEvent;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.messages.MessageStateHandler.Position;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MessageBannerMediator}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(PAUSED)
public class MessageBannerMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int PEEKING_LAYER_HEIGHT = 20;
    private static final int DEFAULT_MARGIN = 18;
    private static final int PEEKING_MARGIN = PEEKING_LAYER_HEIGHT + DEFAULT_MARGIN;

    @Mock private Resources mResources;
    @Mock private DisplayMetrics mDisplayMetrics;
    @Mock private Supplier<Integer> mTopOffsetSupplier;
    @Mock private Supplier<Integer> mMaxTranslationSupplier;
    @Mock private Runnable mDismissedRunnable;
    @Mock private Runnable mShownRunnable;
    @Mock private Runnable mHiddenRunnable;
    @Mock private SwipeAnimationHandler mSwipeAnimationHandler;

    private MessageBannerMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.TITLE, "Title")
                        .with(MessageBannerProperties.DESCRIPTION, "Desc")
                        .build();
        when(mResources.getDisplayMetrics()).thenReturn(mDisplayMetrics);
        mDisplayMetrics.widthPixels = 500;
        when(mResources.getDimensionPixelSize(R.dimen.message_vertical_hide_threshold))
                .thenReturn(16);
        when(mResources.getDimensionPixelSize(R.dimen.message_horizontal_hide_threshold))
                .thenReturn(24);
        when(mResources.getDimensionPixelSize(R.dimen.message_peeking_layer_height))
                .thenReturn(PEEKING_LAYER_HEIGHT);
        when(mResources.getDimensionPixelSize(R.dimen.message_shadow_top_margin))
                .thenReturn(DEFAULT_MARGIN);
        doAnswer(
                        invocation -> {
                            ((Animator) invocation.getArguments()[0]).start();
                            return null;
                        })
                .when(mSwipeAnimationHandler)
                .onSwipeEnd(any(Animator.class));
        mMediator =
                new MessageBannerMediator(
                        mModel,
                        mTopOffsetSupplier,
                        mMaxTranslationSupplier,
                        mResources,
                        mDismissedRunnable,
                        mSwipeAnimationHandler);
        when(mTopOffsetSupplier.get()).thenReturn(75);
        when(mMaxTranslationSupplier.get()).thenReturn(100);
    }

    @Test
    public void testShowMessage() {
        Animator animator = mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable);

        verify(mShownRunnable, times(0)).run();
        assertModelState(0, -75, 0, 0, DEFAULT_MARGIN, "before showing.");

        animator.start();
        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");
        verify(mShownRunnable, times(1)).run();
    }

    @Test
    public void testShowBackMessage() {
        Animator animator = mMediator.show(Position.FRONT, Position.BACK, 0, mShownRunnable);

        verify(mShownRunnable, times(0)).run();
        assertModelState(0, 0, 0, 0, DEFAULT_MARGIN, "before showing.");

        animator.start();
        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, PEEKING_MARGIN, "fully shown.");
        verify(mShownRunnable, times(1)).run();
    }

    @Test
    public void testShowBackMessageWithOffset() {
        Animator animator = mMediator.show(Position.FRONT, Position.BACK, 20, mShownRunnable);

        verify(mShownRunnable, times(0)).run();
        assertModelState(0, 0, 0, 0, DEFAULT_MARGIN, "before showing.");

        animator.start();
        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, PEEKING_MARGIN + 20, "fully shown.");
        verify(mShownRunnable, times(1)).run();
    }

    @Test
    public void testHideMessage() {
        Animator animator = mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable);
        animator.start();

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        animator = mMediator.hide(Position.FRONT, Position.INVISIBLE, true, mHiddenRunnable);
        verify(mHiddenRunnable, times(0)).run();

        animator.start();
        shadowOf(getMainLooper()).idle();

        assertModelState(0, -75, 0, 0, DEFAULT_MARGIN, "after hidden.");
        verify(mHiddenRunnable, times(1)).run();
    }

    @Test
    public void testHideMessageFromBack() {
        Animator animator = mMediator.show(Position.FRONT, Position.BACK, 0, mShownRunnable);
        animator.start();

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, PEEKING_MARGIN, "fully shown.");

        animator = mMediator.hide(Position.BACK, Position.FRONT, true, mHiddenRunnable);
        verify(mHiddenRunnable, times(0)).run();

        animator.start();
        shadowOf(getMainLooper()).idle();

        // because of it is hidden, marginTop is not reset to default margin top
        assertModelState(0, 0, 0, 0, PEEKING_MARGIN, "after hidden.");
        verify(mHiddenRunnable, times(1)).run();
    }

    @Test
    public void testHideMessageNoAnimation() {
        Animator animator = mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable);
        animator.start();

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        mMediator.hide(Position.FRONT, Position.INVISIBLE, false, mHiddenRunnable);

        assertModelState(0, -75, 0, 0, DEFAULT_MARGIN, "after hidden.");
        verify(mHiddenRunnable, times(1)).run();
    }

    @Test
    public void testVerticalDismiss() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // More than the threshold to dismiss
        swipeVertical(-20, 0);

        assertModelState(0, -20, 1 - 20 / 75f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, -75, 0, 0, DEFAULT_MARGIN, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testVerticalNotDismissed() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Less than the threshold to dismiss
        swipeVertical(-10, 0);

        // alpha: 1 - move distance / max translation
        assertModelState(0, -10, 1 - 10 / 75f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        // Should return back to idle position
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "animated to idle position.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testSwipeDownIsNoop() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        swipeVertical(10, 0);

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "swipe doesn't do anything.");

        shadowOf(getMainLooper()).idle();
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testLeftDismiss() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // More than the threshold to dismiss
        swipeHorizontal(-30, 0);

        assertModelState(-30, 0, 1 - 30 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(-500, 0, 0, 1, DEFAULT_MARGIN, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testLeftNotDismissed() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Less than the threshold to dismiss
        swipeHorizontal(-12, 0);

        // Alpha is 1 (fully opaque) - 12 (translationY) / 500 (maxTranslation)
        assertModelState(-12, 0, 1 - 12 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "animated to idle position.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testRightDismiss() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // More than the threshold to dismiss
        swipeHorizontal(30, 0);

        // Alpha is 1 (fully opaque) - 30 (translationY) / 500 (screenWidth)
        assertModelState(30, 0, 1 - 30 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(500, 0, 0, 1, DEFAULT_MARGIN, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testRightNotDismissed() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Less than the threshold to dismiss
        swipeHorizontal(12, 0);

        // Alpha is 1 (fully opaque) - 12 (translationY) / 500 (screen width)
        assertModelState(12, 0, 1 - 12 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "animated to idle position.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testHorizontalFlingFromOutsideThresholdToCenterDismissed() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // More than the threshold to dismiss, fling back to center
        swipeHorizontal(60, -100);

        // Alpha is 1 (fully opaque) - 60 (translationY) / 500 (maxTranslation)
        assertModelState(60, 0, 1 - 60 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(500, 0, 0, 1, DEFAULT_MARGIN, "after swipe");
        verify(mDismissedRunnable).run();
    }

    @Test
    public void testVerticalFlingDown() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // More than the threshold to dismiss, fling back to center
        swipeVertical(-20, 100);

        // Alpha is 1 (fully opaque) - 20 (translationY) / 75 (maxTranslation)
        assertModelState(0, -20, 1 - 20 / 75f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, -75, 0, 0, DEFAULT_MARGIN, "after swipe");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testVerticalFlingDownIsNoop() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Swipe and fling down
        swipeVertical(10, 100);
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "gesture doesn't do anything.");

        shadowOf(getMainLooper()).idle();
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testVerticalFlingUpWithinThresholdDismisses() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Swipe less than threshold and fling up
        swipeVertical(-10, -75);
        // Alpha is 1 (fully opaque) - 10 (translationY) / 75 (maxTranslation)
        assertModelState(0, -10, 1 - 10 / 75f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();
        assertModelState(0, -75, 0, 0, DEFAULT_MARGIN, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testVerticalFlingUpOutsideThresholdDismisses() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Swipe more than threshold and fling up
        swipeVertical(-20, -75);
        // .8 is 1 (fully opaque) - 10 (translationY) / 100 (maxTranslation)
        assertModelState(0, -20, 1 - 20 / 75f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();
        assertModelState(0, -75, 0, 0, DEFAULT_MARGIN, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testLeftFlingWithinThresholdPositiveXNoDismisses() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Less than the threshold to dismiss to the right, fling left
        swipeHorizontal(12, -75);

        // Alpha is 1 (fully opaque) - 12 (translationY) / 500 (maxTranslation: i.e. screen width)
        assertModelState(12, 0, 1 - 12 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "animate back to center.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testLeftFlingWithinThresholdNegativeXNoDismisses() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Less than the threshold to dismiss to the left, fling left
        swipeHorizontal(-12, -75);

        // Alpha is 1 (fully opaque) - 12 (translationY) / 500 (maxTranslation: i.e. screen width)
        assertModelState(-12, 0, 1 - 12 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "animate back to center.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testRightFlingWithinThresholdNegativeXNoDismisses() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Less than the threshold to dismiss to the left, fling right
        swipeHorizontal(-12, 100);

        // Alpha is 1 (fully opaque) - 12 (translationY) / 500 (maxTranslation: i.e. screen width)
        assertModelState(-12, 0, 1 - 12 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "animate back to center.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testRightFlingWithinThresholdPositiveXNoDismisses() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // Less than the threshold to dismiss to the right, fling right
        swipeHorizontal(12, 100);

        // Alpha is 1 (fully opaque) - 12 (translationY) / 500 (maxTranslation: i.e. screen width)
        assertModelState(12, 0, 1 - 12 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "animate back to center.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testLeftFlingOutsideThresholdDismisses() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // More than the threshold to dismiss to the left, fling left
        swipeHorizontal(-30, -75);

        // Alpha is 1 (fully opaque) - 30 (translationY) / 500 (maxTranslation: i.e. screen width)
        assertModelState(-30, 0, 1 - 30 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(-500, 0, 0, 1, DEFAULT_MARGIN, "dismissed to left after fling.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testRightFlingOutsideThresholdDismisses() {
        mMediator.show(Position.INVISIBLE, Position.FRONT, 0, mShownRunnable).start();

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, 1, DEFAULT_MARGIN, "fully shown.");

        // More than the threshold to dismiss to the right, fling right
        swipeHorizontal(30, 100);

        // Alpha is 1 (fully opaque) - 30 (translationY) / 500 (maxTranslation)
        assertModelState(30, 0, 1 - 30 / 500f, 1, DEFAULT_MARGIN, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(500, 0, 0, 1, DEFAULT_MARGIN, "dismissed to right after fling.");
        verify(mDismissedRunnable, times(1)).run();
    }

    /**
     * @param distance Positive is down.
     * @param flingVelocityAtEnd Velocity of the fling gesture at the end; 0 if there is no fling.
     */
    private void swipeVertical(int distance, int flingVelocityAtEnd) {
        MotionEvent e1 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        MotionEvent e2 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 0, distance, 0);

        mMediator.onSwipeStarted(distance < 0 ? ScrollDirection.UP : ScrollDirection.DOWN, e1);
        mMediator.onSwipeUpdated(e2, 0, distance, 0, distance);
        if (flingVelocityAtEnd != 0) {
            MotionEvent e3 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, distance, 0);
            mMediator.onFling(
                    flingVelocityAtEnd < 0 ? ScrollDirection.UP : ScrollDirection.DOWN,
                    e3,
                    0,
                    distance,
                    0,
                    flingVelocityAtEnd);
        }
        mMediator.onSwipeFinished();
    }

    /**
     * @param distance Positive is right.
     * @param flingVelocityAtEnd Velocity of the fling gesture at the end; 0 if there is no fling.
     */
    private void swipeHorizontal(int distance, int flingVelocityAtEnd) {
        MotionEvent e1 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        MotionEvent e2 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, distance, 0, 0);

        mMediator.onSwipeStarted(distance < 0 ? ScrollDirection.LEFT : ScrollDirection.RIGHT, e1);
        mMediator.onSwipeUpdated(e2, distance, 0, distance, 0);
        if (flingVelocityAtEnd != 0) {
            MotionEvent e3 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, distance, 0, 0);
            mMediator.onFling(
                    flingVelocityAtEnd < 0 ? ScrollDirection.LEFT : ScrollDirection.RIGHT,
                    e3,
                    distance,
                    0,
                    flingVelocityAtEnd,
                    0);
        }
        mMediator.onSwipeFinished();
    }

    private void assertModelState(
            float translationXExpected,
            float translationYExpected,
            float alphaExpected,
            float heightExpected,
            int marginTopExpected,
            String message) {
        assertEquals(
                "Incorrect translation x, " + message,
                translationXExpected,
                mModel.get(MessageBannerProperties.TRANSLATION_X),
                MathUtils.EPSILON);
        assertEquals(
                "Incorrect translation y, " + message,
                translationYExpected,
                mModel.get(MessageBannerProperties.TRANSLATION_Y),
                MathUtils.EPSILON);
        assertEquals(
                "Incorrect alpha, " + message,
                alphaExpected,
                mModel.get(MessageBannerProperties.CONTENT_ALPHA),
                MathUtils.EPSILON);
        assertEquals(
                "Incorrect visual height, " + message,
                heightExpected,
                mModel.get(MessageBannerProperties.VISUAL_HEIGHT),
                MathUtils.EPSILON);
        assertEquals(
                "Incorrect margin top, " + message,
                marginTopExpected,
                mModel.get(MessageBannerProperties.MARGIN_TOP));
    }
}
