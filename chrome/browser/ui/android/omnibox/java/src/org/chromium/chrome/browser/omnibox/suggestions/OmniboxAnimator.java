// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.animation.ValueAnimator;

import org.chromium.build.annotations.NullMarked;

/**
 * ValueAnimator subclass for Omnibox animators that provides additional metadata for
 * synchronization.
 */
@NullMarked
public class OmniboxAnimator extends ValueAnimator {
    private final float mStartAlpha;

    public OmniboxAnimator(float startAlpha, long duration) {
        mStartAlpha = startAlpha;
        setDuration(duration);
    }

    public float getStartAlpha() {
        return mStartAlpha;
    }
}
