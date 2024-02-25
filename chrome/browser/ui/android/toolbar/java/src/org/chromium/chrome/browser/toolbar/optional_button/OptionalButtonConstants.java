// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Defines constants used by optional button. */
public class OptionalButtonConstants {
    /**
     * Defines the different transition types for optional button, they are used as arguments to the
     * transition started/finished callbacks.
     */
    @IntDef({
        TransitionType.SWAPPING,
        TransitionType.SHOWING,
        TransitionType.HIDING,
        TransitionType.EXPANDING_ACTION_CHIP,
        TransitionType.COLLAPSING_ACTION_CHIP,
        TransitionType.NONE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TransitionType {
        /** Transitioning from one icon to another. */
        int SWAPPING = 0;

        /** Transitioning from hidden to showing an icon. */
        int SHOWING = 1;

        /** Transitioning from showing an icon to hidden. */
        int HIDING = 2;

        /**
         * Transitioning from showing an icon to showing another as an action chip (rectangular
         * button with a label).
         */
        int EXPANDING_ACTION_CHIP = 3;

        /** Transitioning from an expanded action chip to the same icon as a regular button. */
        int COLLAPSING_ACTION_CHIP = 4;

        int NONE = 5;
    }
}
