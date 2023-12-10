// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;

import androidx.annotation.Nullable;

/** Handler to prepare and trigger swiping animation of {@link MessageStateHandler}. */
public interface SwipeAnimationHandler {
    /** Should be called when swipe is started. */
    void onSwipeStart();

    /**
     * Should be called when swipe is finished.
     * @param animator The animator to trigger after swipe is finished. Set null if no animation
     *                 is required.
     */
    void onSwipeEnd(@Nullable Animator animator);

    /** @return Whether the message view should consume the swipe gesture. */
    boolean isSwipeEnabled();
}
