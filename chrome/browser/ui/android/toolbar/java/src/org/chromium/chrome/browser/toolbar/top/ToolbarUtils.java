// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.top;

import android.animation.ObjectAnimator;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.interpolators.Interpolators;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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

    public static final @ToolbarComponentId int[] RANKED_TOOLBAR_COMPONENTS =
            new int[] {
                ToolbarComponentId.MENU,
                ToolbarComponentId.TAB_SWITCHER,
                ToolbarComponentId.LOCATION_BAR_MINIMUM,
                ToolbarComponentId.PADDING,
                ToolbarComponentId.BACK,
                ToolbarComponentId.INCOGNITO_INDICATOR,
                ToolbarComponentId.ADAPTIVE_BUTTON,
                ToolbarComponentId.RELOAD,
                ToolbarComponentId.FORWARD,
                ToolbarComponentId.HOME,
                ToolbarComponentId.OMNIBOX_BOOKMARK,
                ToolbarComponentId.OMNIBOX_ZOOM,
                ToolbarComponentId.OMNIBOX_INSTALL,
                ToolbarComponentId.OMNIBOX_MIC,
                ToolbarComponentId.OMNIBOX_LENS,
            };

    public static final @ToolbarComponentId int[] APP_MENU_ICON_ROW_COMPONENTS =
            new int[] {
                ToolbarComponentId.RELOAD,
                ToolbarComponentId.FORWARD,
                ToolbarComponentId.OMNIBOX_BOOKMARK
            };

    @IntDef({
        ToolbarComponentId.HOME,
        ToolbarComponentId.BACK,
        ToolbarComponentId.FORWARD,
        ToolbarComponentId.RELOAD,
        ToolbarComponentId.LOCATION_BAR_MINIMUM,
        ToolbarComponentId.OMNIBOX_BOOKMARK,
        ToolbarComponentId.OMNIBOX_ZOOM,
        ToolbarComponentId.OMNIBOX_INSTALL,
        ToolbarComponentId.OMNIBOX_MIC,
        ToolbarComponentId.OMNIBOX_LENS,
        ToolbarComponentId.ADAPTIVE_BUTTON,
        ToolbarComponentId.INCOGNITO_INDICATOR,
        ToolbarComponentId.TAB_SWITCHER,
        ToolbarComponentId.MENU,
        ToolbarComponentId.PADDING,
        ToolbarComponentId.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ToolbarComponentId {
        int HOME = 0;
        int BACK = 1;
        int FORWARD = 2;
        int RELOAD = 3;
        int LOCATION_BAR_MINIMUM = 4;
        int OMNIBOX_BOOKMARK = 5;
        int OMNIBOX_ZOOM = 6;
        int OMNIBOX_INSTALL = 7;
        int OMNIBOX_MIC = 8;
        int OMNIBOX_LENS = 9;
        int ADAPTIVE_BUTTON = 10;
        int INCOGNITO_INDICATOR = 11;
        int TAB_SWITCHER = 12;
        int MENU = 13;
        int PADDING = 14;
        int COUNT = 15;
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
