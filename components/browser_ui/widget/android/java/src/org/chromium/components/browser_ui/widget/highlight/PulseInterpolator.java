// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.highlight;

import android.view.animation.Interpolator;

/**
 * An {@link Interpolator} that pulses a value based on the passed in {@link Interpolator}.  The
 * pulse will fade in and out after a delay.
 */
public class PulseInterpolator implements Interpolator {
    private final Interpolator mInterpolator;

    /**
     * Creates a new {@link PulseInterpolator} instance.
     * @param interpolator The {@link Interpolator} responsible for handling the fade out and in.
     */
    public PulseInterpolator(Interpolator interpolator) {
        mInterpolator = interpolator;
    }

    @Override
    public float getInterpolation(float input) {
        if (input < 0.2) return mInterpolator.getInterpolation(input / 0.2f);
        if (input < 0.6) return 1.f;
        return mInterpolator.getInterpolation(1.f - (input - 0.6f) / 0.4f);
    }
}