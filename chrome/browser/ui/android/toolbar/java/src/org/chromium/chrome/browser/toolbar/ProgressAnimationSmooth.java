// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

/**
 * Progress bar animation logic that smoothly accelerates in the beginning and smoothly decelerates
 * towards the end. The model is applying a constant acceleration followed by a constant
 * deceleration.
 */
class ProgressAnimationSmooth implements ToolbarProgressBar.AnimationLogic {
    // The (de)acceleration unit is progress per second squared where 0 <= progress <= 1.
    private static final float FINISHING_ACCELERATION = 7.0f;
    private static final float ACCELERATION = 0.15f;
    private static final float DECELERATION = 0.15f;

    // Precomputed constants
    private static final float CONSTANT_1 = -1.0f / ACCELERATION;
    private static final float CONSTANT_2 =
            2.0f * DECELERATION / ((DECELERATION + ACCELERATION) * ACCELERATION);
    private static final float CONSTANT_3 =
            DECELERATION / ((DECELERATION + ACCELERATION) * ACCELERATION * ACCELERATION);

    private float mProgress;
    private float mVelocity;

    @Override
    public void reset(float startProgress) {
        mProgress = startProgress;
        mVelocity = 0.0f;
    }

    @Override
    public float updateProgress(float targetProgress, float frameTimeSec, int resolution) {
        final float acceleratingDuration =
                computeAcceleratingDuration(targetProgress, frameTimeSec);
        final float deceleratingDuration = frameTimeSec - acceleratingDuration;

        if (acceleratingDuration > 0.0f) {
            float velocityChange =
                    (targetProgress == 1.0f ? FINISHING_ACCELERATION : ACCELERATION)
                            * acceleratingDuration;
            mProgress += (mVelocity + 0.5f * velocityChange) * acceleratingDuration;
            mVelocity += velocityChange;
        }

        if (deceleratingDuration > 0.0f) {
            float velocityChange = -DECELERATION * deceleratingDuration;
            mProgress += (mVelocity + 0.5f * velocityChange) * deceleratingDuration;
            mVelocity += velocityChange;
        }

        mProgress = Math.min(mProgress, targetProgress);
        if (targetProgress - mProgress < 0.5f / resolution) {
            mProgress = targetProgress;
            mVelocity = 0.0f;
        }

        return mProgress;
    }

    /**
     * Computes and returns accelerating duration.
     *
     * Symbol              Description    Corresponding variable
     *    v_0         Initial velocity                 mVelocity
     *      A             Acceleration              ACCELERATION
     *      D             Deceleration              DECELERATION
     *    d_A    Accelerating duration
     *    d_D    Decelerating duration
     *
     * Given the initial position and the initial velocity, we assume that it accelerates constantly
     * and then decelerates constantly.
     *
     * We want to stop smoothly when it reaches the end. Thus zero velocity at the end:
     * v_0 + A d_A - D d_D = 0
     * Equation image:
     * http://www.HostMath.com/Show.aspx?Code=v_0%20%2B%20A%20d_A%20-%20D%20d_D%20%3D%200
     *
     * The traveled distance should be (targetProgress - mProgress):
     * targetProgress - mProgress =
     *         \int_0^{d_A} (v_0 + A t) dt + \int_0^{d_D} (v_{0} + A d_A - D t) dt
     * Equation image:
     * http://www.HostMath.com/Show.aspx?Code=targetProgress%20-%20mProgress%20%3D%20%5Cint_0%5E%7Bd_A%7D%20(v_0%20%2B%20A%20t)%20dt%20%2B%20%5Cint_0%5E%7Bd_D%7D%20(v_%7B0%7D%20%2B%20A%20d_A%20-%20D%20t)dt
     *
     * This function solves d_A from the above equations.
     */
    private float computeAcceleratingDuration(float targetProgress, float frameTimeSec) {
        if (targetProgress == 1.0f) {
            return frameTimeSec;
        } else {
            float maxAcceleratingDuration =
                    CONSTANT_1 * mVelocity
                            + (float)
                                    Math.sqrt(
                                            CONSTANT_2 * (targetProgress - mProgress)
                                                    + CONSTANT_3 * mVelocity * mVelocity);
            return Math.max(0, Math.min(frameTimeSec, maxAcceleratingDuration));
        }
    }
}
