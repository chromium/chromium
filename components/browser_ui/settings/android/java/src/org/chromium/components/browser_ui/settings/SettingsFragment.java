// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Interface that all fragments for chrome's Settings page should implement. */
@NullMarked
public interface SettingsFragment {
    /**
     * Type of animation of page transition. See also <a
     * href="https://developer.android.com/guide/topics/resources/animation-resource">the reference
     * document</a> for details of each animation.
     */
    @IntDef({AnimationType.TWEEN, AnimationType.PROPERTY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AnimationType {
        /**
         * Uses tween animation. This is legacy and all new classes should use PROPERTY animation.
         */
        // TODO(crbug.com/404074032): Remove this after the migration.
        int TWEEN = 0;

        /** Uses property animation. */
        int PROPERTY = 1;
    }

    /** Returns animation type to be used for fragment transition. */
    public @AnimationType int getAnimationType();
}
