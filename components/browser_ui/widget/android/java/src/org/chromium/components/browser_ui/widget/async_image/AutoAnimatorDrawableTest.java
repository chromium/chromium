// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.async_image;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.PixelFormat;
import android.graphics.drawable.Animatable2;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.GraphicsMode;
import org.robolectric.annotation.GraphicsMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AutoAnimatorDrawable} and {@link ForegroundDrawableCompat}. */
@RunWith(BaseRobolectricTestRunner.class)
@GraphicsMode(Mode.NATIVE)
public class AutoAnimatorDrawableTest {
    private static class TestAnimatedDrawable extends Drawable implements Animatable2 {
        private final List<AnimationCallback> mCallbacks = new ArrayList<>();
        private boolean mRunning;

        @Override
        public void start() {
            mRunning = true;
        }

        @Override
        public void stop() {
            if (!mRunning) return;
            mRunning = false;
            for (AnimationCallback callback : new ArrayList<>(mCallbacks)) {
                callback.onAnimationEnd(this);
            }
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

        @Override
        public void draw(Canvas canvas) {}

        @Override
        public void setAlpha(int alpha) {}

        @Override
        public void setColorFilter(ColorFilter colorFilter) {}

        @Override
        public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }
    }

    private ActivityController<Activity> mActivityController;
    private FrameLayout mParent;
    private ForegroundRoundedCornerImageView mImageView;
    private TestAnimatedDrawable mInnerDrawable;
    private Drawable mWrappedDrawable;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class).setup();
        mActivityController.windowFocusChanged(true);
        Activity activity = mActivityController.get();
        mParent = new FrameLayout(activity);
        activity.setContentView(mParent);
        RobolectricUtil.runAllBackgroundAndUi();
        mImageView = new ForegroundRoundedCornerImageView(activity);
        mInnerDrawable = new TestAnimatedDrawable();
        mWrappedDrawable = AutoAnimatorDrawable.wrap(mInnerDrawable);
        assertNotNull(mWrappedDrawable);
    }

    @Test
    public void testSetVisiblePropagatesToWrappedDrawableAndStopsRestart() {
        mWrappedDrawable.setVisible(true, false);
        assertTrue(mInnerDrawable.isVisible());
        assertTrue(mInnerDrawable.isRunning());

        // Hiding the wrapper stops the inner drawable, which triggers onAnimationEnd().
        mWrappedDrawable.setVisible(false, false);
        assertFalse(mInnerDrawable.isVisible());
        assertFalse(mInnerDrawable.isRunning());

        // Flush the posted AutoRestarter callback; it must not restart a hidden drawable.
        ShadowLooper.idleMainLooper();
        assertFalse(mInnerDrawable.isRunning());
    }

    @Test
    public void testForegroundDrawableVisibilityAndWindowVisibility() {
        mParent.addView(mImageView);
        mImageView.setForegroundDrawableCompat(mWrappedDrawable);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mInnerDrawable.isVisible());
        assertTrue(mInnerDrawable.isRunning());

        mImageView.setVisibility(View.INVISIBLE);
        assertFalse(mInnerDrawable.isVisible());
        assertFalse(mInnerDrawable.isRunning());

        mImageView.setVisibility(View.VISIBLE);
        assertTrue(mInnerDrawable.isVisible());
        assertTrue(mInnerDrawable.isRunning());

        mImageView.onWindowVisibilityChanged(View.GONE);
        assertFalse(mInnerDrawable.isVisible());
        assertFalse(mInnerDrawable.isRunning());
    }

    @Test
    public void testForegroundDrawableStopsOnDetachWhileWindowGone() {
        mParent.addView(mImageView);
        mImageView.setForegroundDrawableCompat(mWrappedDrawable);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mInnerDrawable.isVisible());
        assertTrue(mInnerDrawable.isRunning());

        // Simulate window stopping (windowVisibility -> GONE) followed by detach on destroy.
        mActivityController.stop();
        mParent.removeView(mImageView);
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(mInnerDrawable.isVisible());
        assertFalse(mInnerDrawable.isRunning());
    }

    @Test
    public void testSetDrawableSetsCallbackBeforeUpdatingVisibility() {
        mParent.addView(mImageView);
        TestAnimatedDrawable callbackCheckingDrawable =
                new TestAnimatedDrawable() {
                    @Override
                    public boolean setVisible(boolean visible, boolean restart) {
                        assertEquals(mImageView, getCallback());
                        return super.setVisible(visible, restart);
                    }
                };
        mImageView.setForegroundDrawableCompat(callbackCheckingDrawable);
        assertEquals(mImageView, callbackCheckingDrawable.getCallback());
    }
}
