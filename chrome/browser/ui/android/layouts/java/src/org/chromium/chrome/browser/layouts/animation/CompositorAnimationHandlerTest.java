// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts.animation;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link CompositorAnimator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CompositorAnimationHandlerTest {
    private static final long FAST_DURATION_MS = 100;
    private static final long SLOW_DURATION_MS = 1000;

    private CompositorAnimationHandler mAnimations;

    @Test
    @SmallTest
    public void testConcurrentAnimationsFinishSeparately() {
        mAnimations = new CompositorAnimationHandler(CallbackUtils.emptyRunnable());

        CompositorAnimator mFastAnimation =
                CompositorAnimator.ofFloat(mAnimations, 0.f, 1.f, FAST_DURATION_MS, null);
        CompositorAnimator mSlowAnimation =
                CompositorAnimator.ofFloat(mAnimations, 0.f, 1.f, SLOW_DURATION_MS, null);

        mFastAnimation.start();
        mSlowAnimation.start();

        CompositorAnimationHandler.setTestingMode(true);

        // Advances time to check that the fast animation will finish first.
        mAnimations.pushUpdateInTestingMode(1 + FAST_DURATION_MS);
        Assert.assertFalse(mFastAnimation.isRunning());
        Assert.assertTrue(mSlowAnimation.isRunning());

        // Advances time to check that all animations are finished.
        mAnimations.pushUpdateInTestingMode(1 + SLOW_DURATION_MS);
        Assert.assertFalse(mFastAnimation.isRunning());
        Assert.assertFalse(mSlowAnimation.isRunning());
    }
}
