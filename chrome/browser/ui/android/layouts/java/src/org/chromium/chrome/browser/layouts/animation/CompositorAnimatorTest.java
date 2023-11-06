// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts.animation;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.os.Build;
import android.os.Looper;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.LinearInterpolator;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;

import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for the {@link CompositorAnimator} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        // AnimatorSet seems to not work in Robolectric 3.4.2. Remove this SDK
        // specification once we upgrade to a version in which it works. crbug.com/774357
        sdk = Build.VERSION_CODES.TIRAMISU)
public final class CompositorAnimatorTest {
    /** An animation update listener that counts calls to its methods. */
    private static class TestUpdateListener implements CompositorAnimator.AnimatorUpdateListener {
        private final CallbackHelper mUpdateCallbackHelper = new CallbackHelper();
        private float mLastAnimatedFraction;

        @Override
        public void onAnimationUpdate(CompositorAnimator animator) {
            mLastAnimatedFraction = animator.getAnimatedFraction();
            mUpdateCallbackHelper.notifyCalled();
        }
    }

    /** An animation listener for tracking lifecycle events on an animator. */
    private static class TestAnimatorListener extends AnimatorListenerAdapter {
        private final CallbackHelper mCancelCallbackHelper = new CallbackHelper();
        private final CallbackHelper mEndCallbackHelper = new CallbackHelper();
        private final CallbackHelper mStartCallbackHelper = new CallbackHelper();

        @Override
        public void onAnimationCancel(Animator animation) {
            mCancelCallbackHelper.notifyCalled();
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            mEndCallbackHelper.notifyCalled();
        }

        @Override
        public void onAnimationStart(Animator animation) {
            mStartCallbackHelper.notifyCalled();
        }
    }

    private final CallbackHelper mRequestRenderCallbackHelper = new CallbackHelper();

    /** The handler that is responsible for managing all {@link CompositorAnimator}s. */
    private CompositorAnimationHandler mHandler;

    /** A listener for updates to an animation. */
    private TestUpdateListener mUpdateListener;

    /** A listener for animation lifecycle events. */
    private TestAnimatorListener mListener;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mHandler = new CompositorAnimationHandler(mRequestRenderCallbackHelper::notifyCalled);

        mUpdateListener = new TestUpdateListener();
        mListener = new TestAnimatorListener();
    }

    @Test
    public void testUnityScale() {
        // Make sure the testing environment doesn't have ANIMATOR_DURATION_SCALE set to a value
        // other than 1.
        assertEquals(CompositorAnimator.sDurationScale, 1, 0);
    }

    @Test
    public void testAnimationStarted() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        animator.addListener(mListener);

        assertEquals(
                "No updates should have been requested.",
                0,
                mRequestRenderCallbackHelper.getCallCount());
        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'start' event should not have been called.",
                0,
                mListener.mStartCallbackHelper.getCallCount());

        animator.start();

        assertEquals(
                "One update should have been requested.",
                1,
                mRequestRenderCallbackHelper.getCallCount());
        assertEquals(
                "There should be one active animation.", 1, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'start' event should have been called.",
                1,
                mListener.mStartCallbackHelper.getCallCount());
    }

    @Test
    public void testAnimationEnd() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        animator.addListener(mListener);

        assertEquals(
                "No updates should have been requested.",
                0,
                mRequestRenderCallbackHelper.getCallCount());
        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'end' event should not have been called.",
                0,
                mListener.mEndCallbackHelper.getCallCount());

        animator.start();

        assertEquals(
                "One update should have been requested.",
                1,
                mRequestRenderCallbackHelper.getCallCount());
        assertEquals(
                "There should be one active animation.", 1, mHandler.getActiveAnimationCount());

        mHandler.pushUpdate(15);

        assertEquals(
                "Two updates should have been requested",
                2,
                mRequestRenderCallbackHelper.getCallCount());
        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'cancel' event should not have been called.",
                0,
                mListener.mCancelCallbackHelper.getCallCount());
        assertEquals(
                "The 'end' event should have been called.",
                1,
                mListener.mEndCallbackHelper.getCallCount());
    }

    @Test
    public void testAnimationCancel() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        animator.addListener(mListener);

        assertEquals(
                "No updates should have been requested.",
                0,
                mRequestRenderCallbackHelper.getCallCount());
        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'end' event should not have been called.",
                0,
                mListener.mEndCallbackHelper.getCallCount());

        animator.start();

        assertEquals(
                "One update should have been requested.",
                1,
                mRequestRenderCallbackHelper.getCallCount());
        assertEquals(
                "There should be one active animation.", 1, mHandler.getActiveAnimationCount());

        animator.cancel();

        assertEquals(
                "One update should have been requested.",
                1,
                mRequestRenderCallbackHelper.getCallCount());
        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'cancel' event should have been called.",
                1,
                mListener.mCancelCallbackHelper.getCallCount());
        assertEquals(
                "The 'end' event should have been called.",
                1,
                mListener.mEndCallbackHelper.getCallCount());
    }

    @Test
    public void testAnimationValue() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        animator.setValues(50, 100);
        LinearInterpolator interpolator = new LinearInterpolator();
        animator.setInterpolator(interpolator);

        animator.start();

        assertEquals(
                "The animated value is incorrect.",
                50,
                animator.getAnimatedValue(),
                MathUtils.EPSILON);

        mHandler.pushUpdate(5);

        assertEquals(
                "The animated value is incorrect.",
                75,
                animator.getAnimatedValue(),
                MathUtils.EPSILON);

        mHandler.pushUpdate(5);

        assertEquals(
                "The animated value is incorrect.",
                100,
                animator.getAnimatedValue(),
                MathUtils.EPSILON);

        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
    }

    @Test
    public void testAnimationDynamicValue() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        final AtomicInteger startValue = new AtomicInteger(50);
        final AtomicInteger endValue = new AtomicInteger(100);

        animator.setValues(startValue::floatValue, endValue::floatValue);
        LinearInterpolator interpolator = new LinearInterpolator();
        animator.setInterpolator(interpolator);

        animator.start();

        assertEquals(
                "The animated value is incorrect.",
                50,
                animator.getAnimatedValue(),
                MathUtils.EPSILON);

        mHandler.pushUpdate(5);

        assertEquals(
                "The animated value is incorrect.",
                75,
                animator.getAnimatedValue(),
                MathUtils.EPSILON);

        startValue.set(0);
        endValue.set(20);
        assertEquals(
                "The animated value is incorrect.",
                10,
                animator.getAnimatedValue(),
                MathUtils.EPSILON);

        mHandler.pushUpdate(5);

        assertEquals(
                "The animated value is incorrect.",
                20,
                animator.getAnimatedValue(),
                MathUtils.EPSILON);

        startValue.set(200);
        endValue.set(300);
        assertEquals(
                "The animated value is incorrect.",
                300,
                animator.getAnimatedValue(),
                MathUtils.EPSILON);

        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
    }

    @Test
    public void testAnimationCancelFraction() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        animator.addUpdateListener(mUpdateListener);
        LinearInterpolator interpolator = new LinearInterpolator();
        animator.setInterpolator(interpolator);

        animator.start();

        mHandler.pushUpdate(5);

        animator.cancel();

        // Calling 'cancel' should leave the value in its current state.
        assertEquals(
                "The animated fraction is incorrect.",
                0.5f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);
        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
    }

    @Test
    public void testAnimationEndFraction() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        animator.addUpdateListener(mUpdateListener);
        LinearInterpolator interpolator = new LinearInterpolator();
        animator.setInterpolator(interpolator);

        animator.start();

        mHandler.pushUpdate(5);

        animator.end();

        // Calling 'end' should set the value to its final state.
        assertEquals(
                "The animated fraction is incorrect.",
                1f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);
        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
    }

    @Test
    public void testAnimationUpdate_linear() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(100);
        animator.addUpdateListener(mUpdateListener);

        LinearInterpolator interpolator = new LinearInterpolator();
        animator.setInterpolator(interpolator);

        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());

        animator.start();

        assertEquals(
                "There should be one active animation.", 1, mHandler.getActiveAnimationCount());
        assertEquals(
                "The animated fraction is incorrect.",
                0f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);

        mHandler.pushUpdate(10);
        assertEquals(
                "The animated fraction is incorrect.",
                0.1f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);
        assertEquals(
                "The update event count is incorrect.",
                1,
                mUpdateListener.mUpdateCallbackHelper.getCallCount());

        mHandler.pushUpdate(80);
        assertEquals(
                "The animated fraction is incorrect.",
                0.9f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);
        assertEquals(
                "The update event count is incorrect.",
                2,
                mUpdateListener.mUpdateCallbackHelper.getCallCount());

        mHandler.pushUpdate(10);
        assertEquals(
                "The animated fraction is incorrect.",
                1f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);
        assertEquals(
                "The update event count is incorrect.",
                3,
                mUpdateListener.mUpdateCallbackHelper.getCallCount());

        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
    }

    @Test
    public void testAnimationUpdate_nonLinear() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(100);
        animator.addUpdateListener(mUpdateListener);

        AccelerateDecelerateInterpolator interpolator = new AccelerateDecelerateInterpolator();
        animator.setInterpolator(interpolator);

        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());

        animator.start();

        assertEquals(
                "There should be one active animation.", 1, mHandler.getActiveAnimationCount());
        assertEquals(
                "The animated fraction is incorrect.",
                0f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);

        mHandler.pushUpdate(10);
        assertEquals(
                "The animated fraction is incorrect.",
                0.0245f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);
        assertEquals(
                "The update event count is incorrect.",
                1,
                mUpdateListener.mUpdateCallbackHelper.getCallCount());

        mHandler.pushUpdate(80);
        assertEquals(
                "The animated fraction is incorrect.",
                0.9755f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);
        assertEquals(
                "The update event count is incorrect.",
                2,
                mUpdateListener.mUpdateCallbackHelper.getCallCount());

        mHandler.pushUpdate(10);
        assertEquals(
                "The animated fraction is incorrect.",
                1f,
                mUpdateListener.mLastAnimatedFraction,
                MathUtils.EPSILON);
        assertEquals(
                "The update event count is incorrect.",
                3,
                mUpdateListener.mUpdateCallbackHelper.getCallCount());

        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
    }

    @Test
    public void testAnimatorSet_playTogether() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        animator.addListener(mListener);

        CompositorAnimator animator2 = new CompositorAnimator(mHandler);
        animator2.setDuration(10);
        TestAnimatorListener listener2 = new TestAnimatorListener();
        animator2.addListener(listener2);

        ArrayList<Animator> animatorList = new ArrayList<>();
        animatorList.add(animator);
        animatorList.add(animator2);

        AnimatorSet animatorSet = new AnimatorSet();
        TestAnimatorListener setListener = new TestAnimatorListener();
        animatorSet.addListener(setListener);
        animatorSet.playTogether(animatorList);

        animatorSet.start();

        assertEquals(
                "There should be two active animations.", 2, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'start' event should have been called for the set listener.",
                1,
                setListener.mStartCallbackHelper.getCallCount());
        assertEquals(
                "The 'start' event should have been called for the first listener.",
                1,
                mListener.mStartCallbackHelper.getCallCount());
        assertEquals(
                "The 'start' event should have been called for the second listener.",
                1,
                listener2.mStartCallbackHelper.getCallCount());

        mHandler.pushUpdate(15);
        shadowOf(Looper.getMainLooper()).idle();

        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'end' event should have been called for the set listener.",
                1,
                setListener.mEndCallbackHelper.getCallCount());
        assertEquals(
                "The 'end' event should have been called for the first listener.",
                1,
                mListener.mEndCallbackHelper.getCallCount());
        assertEquals(
                "The 'end' event should have been called for the second listener.",
                1,
                listener2.mEndCallbackHelper.getCallCount());
    }

    @DisabledTest(message = "crbug.com/774357")
    @Test
    public void testAnimatorSet_playSequentially() {
        CompositorAnimator animator = new CompositorAnimator(mHandler);
        animator.setDuration(10);
        animator.addListener(mListener);

        CompositorAnimator animator2 = new CompositorAnimator(mHandler);
        animator2.setDuration(10);
        TestAnimatorListener listener2 = new TestAnimatorListener();
        animator2.addListener(listener2);

        ArrayList<Animator> animatorList = new ArrayList<>();
        animatorList.add(animator);
        animatorList.add(animator2);

        AnimatorSet animatorSet = new AnimatorSet();
        TestAnimatorListener setListener = new TestAnimatorListener();
        animatorSet.addListener(setListener);
        animatorSet.playSequentially(animatorList);

        animatorSet.start();

        assertEquals(
                "There should be one active animation.", 1, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'start' event should have been called for the set listener.",
                1,
                setListener.mStartCallbackHelper.getCallCount());
        assertEquals(
                "The 'start' event should have been called for the first listener.",
                1,
                mListener.mStartCallbackHelper.getCallCount());
        assertEquals(
                "The 'start' event should not have been called for the second listener.",
                0,
                listener2.mStartCallbackHelper.getCallCount());

        mHandler.pushUpdate(15);

        assertEquals(
                "There should be one active animation.", 1, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'end' event should not have been called for the set listener.",
                0,
                setListener.mEndCallbackHelper.getCallCount());
        assertEquals(
                "The 'end' event should have been called for the first listener.",
                1,
                mListener.mEndCallbackHelper.getCallCount());
        assertEquals(
                "The 'start' event should have been called for the second listener.",
                1,
                listener2.mStartCallbackHelper.getCallCount());

        mHandler.pushUpdate(15);

        assertEquals(
                "There should be no active animations.", 0, mHandler.getActiveAnimationCount());
        assertEquals(
                "The 'end' event should have been called for the set listener.",
                1,
                setListener.mEndCallbackHelper.getCallCount());
        assertEquals(
                "The 'end' event should have been called for the second listener.",
                1,
                listener2.mEndCallbackHelper.getCallCount());
    }
}
