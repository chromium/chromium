// Copyright 2021 The Chromium Authors. All rights reserved.
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

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MessageBannerMediator}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(PAUSED)
public class MessageBannerMediatorUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Resources mResources;
    @Mock
    private DisplayMetrics mDisplayMetrics;
    @Mock
    private Supplier<Integer> mMaxTranslationSupplier;
    @Mock
    private Runnable mDismissedRunnable;
    @Mock
    private Runnable mShownRunnable;
    @Mock
    private Runnable mHiddenRunnable;
    @Mock
    private Callback<Animator> mAnimatorStartCallback;

    private MessageBannerMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                         .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
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
        when(mResources.getDimensionPixelSize(R.dimen.message_max_horizontal_translation))
                .thenReturn(120);
        doAnswer(invocation -> {
            ((Animator) invocation.getArguments()[0]).start();
            return null;
        })
                .when(mAnimatorStartCallback)
                .onResult(any(Animator.class));
        mMediator = new MessageBannerMediator(mModel, mMaxTranslationSupplier, mResources,
                mDismissedRunnable, mAnimatorStartCallback);
        when(mMaxTranslationSupplier.get()).thenReturn(100);
    }

    @Test
    public void testShowMessage() {
        mMediator.show(mShownRunnable);

        verify(mShownRunnable, times(0)).run();
        assertModelState(0, -100, 0, "before showing.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "fully shown.");
        verify(mShownRunnable, times(1)).run();
    }

    @Test
    public void testHideMessage() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "fully shown.");

        mMediator.hide(true, mHiddenRunnable);
        verify(mHiddenRunnable, times(0)).run();

        shadowOf(getMainLooper()).idle();

        assertModelState(0, -100, 0, "after hidden.");
        verify(mHiddenRunnable, times(1)).run();
    }

    @Test
    public void testHideMessageNoAnimation() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "fully shown.");

        mMediator.hide(false, mHiddenRunnable);
        assertModelState(0, -100, 0, "after hidden.");
        verify(mHiddenRunnable, times(1)).run();
    }

    @Test
    public void testVerticalDismiss() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // More than the threshold to dismiss
        swipeVertical(-20, 0);

        // .8 is 1 (fully opaque) - 20 (translationY) / 100 (maxTranslation)
        assertModelState(0, -20, .8f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, -100, 0, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testVerticalNotDismissed() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Less than the threshold to dismiss
        swipeVertical(-10, 0);

        // .9 is 1 (fully opaque) - 20 (translationY) / 100 (maxTranslation)
        assertModelState(0, -10, .9f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        // Should return back to idle position
        assertModelState(0, 0, 1, "animated to idle position.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testSwipeDownIsNoop() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        swipeVertical(10, 0);

        assertModelState(0, 0, 1, "swipe doesn't do anything.");

        shadowOf(getMainLooper()).idle();
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testLeftDismiss() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // More than the threshold to dismiss
        swipeHorizontal(-30, 0);

        // .75 is 1 (fully opaque) - 30 (translationX) / 120 (maxTranslation)
        assertModelState(-30, 0, .75f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(-120, 0, 0, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testLeftNotDismissed() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Less than the threshold to dismiss
        swipeHorizontal(-12, 0);

        // Alpha .9 is 1 (fully opaque) - 12 (translationY) / 120 (maxTranslation)
        assertModelState(-12, 0, .9f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "animated to idle position.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testRightDismiss() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // More than the threshold to dismiss
        swipeHorizontal(30, 0);

        // Alpha .75 is 1 (fully opaque) - 30 (translationY) / 120 (maxTranslation)
        assertModelState(30, 0, .75f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(120, 0, 0, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testRightNotDismissed() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Less than the threshold to dismiss
        swipeHorizontal(12, 0);

        // Alpha .9 is 1 (fully opaque) - 12 (translationY) / 120 (maxTranslation)
        assertModelState(12, 0, .9f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "animated to idle position.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testHorizontalFlingFromOutsideThresholdToCenterDismissed() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // More than the threshold to dismiss, fling back to center
        swipeHorizontal(60, -100);

        // Alpha .5 is 1 (fully opaque) - 60 (translationY) / 120 (maxTranslation)
        assertModelState(60, 0, .5f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(120, 0, 0, "after swipe");
        verify(mDismissedRunnable).run();
    }

    @Test
    public void testVerticalFlingDown() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // More than the threshold to dismiss, fling back to center
        swipeVertical(-20, 100);

        // .8 is 1 (fully opaque) - 20 (translationY) / 100 (maxTranslation)
        assertModelState(0, -20, .8f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, -100, 0, "after swipe");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testVerticalFlingDownIsNoop() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Swipe and fling down
        swipeVertical(10, 100);
        assertModelState(0, 0, 1, "gesture doesn't do anything.");

        shadowOf(getMainLooper()).idle();
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testVerticalFlingUpWithinThresholdDismisses() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Swipe less than threshold and fling up
        swipeVertical(-10, -100);
        // .9 is 1 (fully opaque) - 10 (translationY) / 100 (maxTranslation)
        assertModelState(0, -10, .9f, "after swipe.");

        shadowOf(getMainLooper()).idle();
        assertModelState(0, -100, 0, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testVerticalFlingUpOutsideThresholdDismisses() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Swipe more than threshold and fling up
        swipeVertical(-20, -100);
        // .8 is 1 (fully opaque) - 10 (translationY) / 100 (maxTranslation)
        assertModelState(0, -20, .8f, "after swipe.");

        shadowOf(getMainLooper()).idle();
        assertModelState(0, -100, 0, "after dismiss animation.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testLeftFlingWithinThresholdPositiveXNoDismisses() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Less than the threshold to dismiss to the right, fling left
        swipeHorizontal(12, -100);

        // Alpha .9 is 1 (fully opaque) - 12 (translationY) / 120 (maxTranslation)
        assertModelState(12, 0, .9f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "animate back to center.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testLeftFlingWithinThresholdNegativeXNoDismisses() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Less than the threshold to dismiss to the left, fling left
        swipeHorizontal(-12, -100);

        // Alpha .9 is 1 (fully opaque) - 12 (translationY) / 120 (maxTranslation)
        assertModelState(-12, 0, .9f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "animate back to center.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testRightFlingWithinThresholdNegativeXNoDismisses() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Less than the threshold to dismiss to the left, fling right
        swipeHorizontal(-12, 100);

        // Alpha .9 is 1 (fully opaque) - 12 (translationY) / 120 (maxTranslation)
        assertModelState(-12, 0, .9f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "animate back to center.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testRightFlingWithinThresholdPositiveXNoDismisses() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // Less than the threshold to dismiss to the right, fling right
        swipeHorizontal(12, 100);

        // Alpha .9 is 1 (fully opaque) - 12 (translationY) / 120 (maxTranslation)
        assertModelState(12, 0, .9f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(0, 0, 1, "animate back to center.");
        verify(mDismissedRunnable, times(0)).run();
    }

    @Test
    public void testLeftFlingOutsideThresholdDismisses() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // More than the threshold to dismiss to the left, fling left
        swipeHorizontal(-30, -100);

        // Alpha .75 is 1 (fully opaque) - 30 (translationY) / 120 (maxTranslation)
        assertModelState(-30, 0, .75f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(-120, 0, 0, "dismissed to left after fling.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testRightFlingOutsideThresholdDismisses() {
        mMediator.show(mShownRunnable);

        shadowOf(getMainLooper()).idle();

        verify(mDismissedRunnable, times(0)).run();
        assertModelState(0, 0, 1, "fully shown.");

        // More than the threshold to dismiss to the right, fling right
        swipeHorizontal(30, 100);

        // Alpha .75 is 1 (fully opaque) - 30 (translationY) / 120 (maxTranslation)
        assertModelState(30, 0, .75f, "after swipe.");

        shadowOf(getMainLooper()).idle();

        assertModelState(120, 0, 0, "dismissed to right after fling.");
        verify(mDismissedRunnable, times(1)).run();
    }

    @Test
    public void testHorizontalTranslationSupplier() {
        // Minimum of max translation dimen (120) and half the screen width (500/2 = 250).
        assertEquals("Wrong initial max horizontal translation.", 120,
                mMediator.getMaxHorizontalTranslationSupplierForTesting().get(), MathUtils.EPSILON);

        // Update the screen width to 200
        mDisplayMetrics.widthPixels = 200;

        // Minimum of max translation dimen (120) and half the screen width (200/2 = 100).
        assertEquals("Max horizontal translation isn't updated width screen width.", 100,
                mMediator.getMaxHorizontalTranslationSupplierForTesting().get(), MathUtils.EPSILON);
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
            mMediator.onFling(flingVelocityAtEnd < 0 ? ScrollDirection.UP : ScrollDirection.DOWN,
                    e3, 0, distance, 0, flingVelocityAtEnd);
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
            mMediator.onFling(flingVelocityAtEnd < 0 ? ScrollDirection.LEFT : ScrollDirection.RIGHT,
                    e3, distance, 0, flingVelocityAtEnd, 0);
        }
        mMediator.onSwipeFinished();
    }

    private void assertModelState(float translationXExpected, float translationYExpected,
            float alphaExpected, String message) {
        assertEquals("Incorrect translation x, " + message, translationXExpected,
                mModel.get(MessageBannerProperties.TRANSLATION_X), MathUtils.EPSILON);
        assertEquals("Incorrect translation y, " + message, translationYExpected,
                mModel.get(MessageBannerProperties.TRANSLATION_Y), MathUtils.EPSILON);
        assertEquals("Incorrect alpha, " + message, alphaExpected,
                mModel.get(MessageBannerProperties.ALPHA), MathUtils.EPSILON);
    }
}
