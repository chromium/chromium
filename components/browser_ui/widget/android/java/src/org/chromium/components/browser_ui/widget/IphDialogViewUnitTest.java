// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.drawable.Animatable2.AnimationCallback;
import android.graphics.drawable.AnimatedVectorDrawable;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link IphDialogView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IphDialogViewUnitTest {
    private Activity mActivity;
    private FrameLayout mRootView;
    private IphDialogView mIphDialogView;
    private TestAnimatedDrawable mDrawable;

    private static class TestAnimatedDrawable extends AnimatedVectorDrawable {
        private final List<AnimationCallback> mCallbacks = new ArrayList<>();
        private boolean mRunning;
        private int mStartCount;
        private int mStopCount;

        @Override
        public void start() {
            mRunning = true;
            mStartCount++;
        }

        @Override
        public void stop() {
            mRunning = false;
            mStopCount++;
        }

        @Override
        public boolean isRunning() {
            return mRunning;
        }

        @Override
        public void registerAnimationCallback(AnimationCallback callback) {
            mCallbacks.add(callback);
        }

        @Override
        public boolean unregisterAnimationCallback(AnimationCallback callback) {
            return mCallbacks.remove(callback);
        }

        @Override
        public void clearAnimationCallbacks() {
            mCallbacks.clear();
        }

        void finishAnimation() {
            mRunning = false;
            for (AnimationCallback callback : new ArrayList<>(mCallbacks)) {
                callback.onAnimationEnd(this);
            }
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRootView = new FrameLayout(mActivity);
        mRootView.setBottom(1000);
        mActivity.setContentView(mRootView);

        mIphDialogView =
                (IphDialogView)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        R.layout.iph_dialog_layout,
                                        mRootView,
                                        /* attachToRoot= */ false);
        mIphDialogView.setRootView(mRootView);
        mDrawable = new TestAnimatedDrawable();
        mIphDialogView.initialize(mDrawable, "Title", "Description");
    }

    @Test
    public void testStopIphAnimation_CancelsDelayedRestartCallback() {
        mIphDialogView.setIntervalMs(1500);
        mIphDialogView.startIphAnimation();
        assertTrue(mDrawable.isRunning());
        assertEquals(1, mDrawable.mStartCount);
        assertEquals(1, mDrawable.mCallbacks.size());

        // Trigger onAnimationEnd, which schedules a delayed restart after 1500ms.
        mDrawable.finishAnimation();
        assertFalse(mDrawable.isRunning());

        // Stopping the animation before the 1500ms interval expires must cancel the pending post.
        mIphDialogView.stopIphAnimation();
        assertEquals(0, mDrawable.mCallbacks.size());
        assertEquals(1, mDrawable.mStopCount);
        ShadowLooper.idleMainLooper(2000, TimeUnit.MILLISECONDS);
        assertFalse(mDrawable.isRunning());
        assertEquals(1, mDrawable.mStartCount);
    }

    @Test
    public void testOnDetachedFromWindow_StopsAnimationAndCancelsCallback() {
        mIphDialogView.setIntervalMs(1500);
        mRootView.addView(mIphDialogView);
        assertNotNull(mIphDialogView.getWindowToken());

        mIphDialogView.startIphAnimation();
        mDrawable.finishAnimation();

        // Detaching from window must call stopIphAnimation() and cancel the pending restart.
        mRootView.removeView(mIphDialogView);
        assertEquals(0, mDrawable.mCallbacks.size());
        assertEquals(1, mDrawable.mStopCount);
        ShadowLooper.idleMainLooper(2000, TimeUnit.MILLISECONDS);
        assertFalse(mDrawable.isRunning());
        assertEquals(1, mDrawable.mStartCount);
    }
}
