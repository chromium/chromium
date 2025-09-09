// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.ObjectAnimator;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.interpolators.Interpolators;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;

@NullMarked
public class ToolbarUtils {
    private static final int ICON_FADE_IN_ANIMATION_DELAY_MS = 75;
    private static final int ICON_FADE_ANIMATION_DURATION_MS = 150;

    /** Returns the id for the appropriate toolbar icon ripple drawable. */
    public static int getToolbarIconRippleId(boolean isIncognito) {
        return isIncognito
                ? org.chromium.chrome.browser.toolbar.R.drawable.default_icon_background_baseline
                : org.chromium.chrome.browser.toolbar.R.drawable.default_icon_background;
    }

    public static boolean isToolbarTabletResizeRefactorEnabled() {
        return ChromeFeatureList.sToolbarTabletResizeRefactor.isEnabled();
    }

    /**
     * This interface should be implemented by toolbar components to consume available width from
     * the toolbar to display themselves.
     */
    public interface ToolbarWidthConsumer {
        /**
         * Takes in the remaining width available in the toolbar for displaying {@link ToolbarChild}
         * components. This ToolbarChild will display itself using the available width, if
         * appropriate, returning the width it has consumed for itself. Returning 0 indicates that
         * this ToolbarChild is not showing, or cannot be shown.
         *
         * @param availableWidth The available width in the toolbar for the button to display
         *     itself.
         * @return The width used to display this ToolbarChild.
         */
        int updateVisibility(int availableWidth);

        /**
         * Takes in the remaining width available in the toolbar for displaying {@link ToolbarChild}
         * components. This ToolbarChild will display itself using the available width, if
         * appropriate, returning the width it has consumed for itself. Returning 0 indicates that
         * this ToolbarChild is not showing, or cannot be shown.
         *
         * <p>This ToolbarChild will build a new animation for its visibility change, if applicable,
         * and add it to the supplied list of animators.
         *
         * @param availableWidth The available width in the toolbar for the button to display
         *     itself.
         * @param animators The collection of {@link Animator}s used to animate a change in the
         *     toolbar.
         * @return The width used to display this ToolbarChild.
         */
        int updateVisibilityWithAnimation(int availableWidth, Collection<Animator> animators);
    }

    public static final @ToolbarComponentId int[] RANKED_TOOLBAR_COMPONENTS =
            new int[] {
                ToolbarComponentId.MENU,
                ToolbarComponentId.BACK,
                ToolbarComponentId.TAB_SWITCHER,
                ToolbarComponentId.PADDING,
                ToolbarComponentId.ADAPTIVE_BUTTON,
                ToolbarComponentId.RELOAD,
                ToolbarComponentId.FORWARD,
                ToolbarComponentId.HOME,
            };

    @IntDef({
        ToolbarComponentId.HOME,
        ToolbarComponentId.BACK,
        ToolbarComponentId.FORWARD,
        ToolbarComponentId.RELOAD,
        ToolbarComponentId.ADAPTIVE_BUTTON,
        ToolbarComponentId.TAB_SWITCHER,
        ToolbarComponentId.MENU,
        ToolbarComponentId.PADDING,
        ToolbarComponentId.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ToolbarComponentId {
        int HOME = 0;
        int BACK = 1;
        int FORWARD = 2;
        int RELOAD = 3;
        int ADAPTIVE_BUTTON = 4;
        int TAB_SWITCHER = 5;
        int MENU = 6;
        int PADDING = 7;
        int COUNT = 8;
    }

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
