// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.top;

import android.animation.ObjectAnimator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.interpolators.Interpolators;

@NullMarked
public class ToolbarUtils {
    private static final int ICON_FADE_IN_ANIMATION_DELAY_MS = 75;
    private static final int ICON_FADE_ANIMATION_DURATION_MS = 150;

    /**
     * Sets values in the animator (interpolator, duration, etc) for fading in animations. Returns
     * the input {@link ObjectAnimator}.
     */
    public static ObjectAnimator asFadeInAnimation(ObjectAnimator objectAnimator) {
        objectAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        objectAnimator.setStartDelay(ICON_FADE_IN_ANIMATION_DELAY_MS);
        objectAnimator.setDuration(ICON_FADE_ANIMATION_DURATION_MS);
        return objectAnimator;
    }

    /**
     * Sets values in the animator (interpolator, duration, etc) for fading out animations. Returns
     * the input {@link ObjectAnimator}.
     */
    public static ObjectAnimator asFadeOutAnimation(ObjectAnimator objectAnimator) {
        objectAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        objectAnimator.setDuration(ICON_FADE_ANIMATION_DURATION_MS);
        return objectAnimator;
    }
}
