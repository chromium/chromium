// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.top;

import android.animation.ObjectAnimator;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.R;
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
                ? R.drawable.default_icon_background_baseline
                : R.drawable.default_icon_background;
    }

    public static boolean isToolbarTabletResizeRefactorEnabled() {
        return ChromeFeatureList.sToolbarTabletResizeRefactor.isEnabled();
    }

    public static final @ToolbarComponentId int[] RANKED_TOOLBAR_COMPONENTS =
            new int[] {
                ToolbarComponentId.PADDING,
                ToolbarComponentId.MENU,
                ToolbarComponentId.TAB_SWITCHER,
                ToolbarComponentId.LOCATION_BAR_MINIMUM,
                ToolbarComponentId.POPPED_EXTENSION_ACTION,
                ToolbarComponentId.BACK,
                ToolbarComponentId.INCOGNITO_INDICATOR,
                ToolbarComponentId.SIGNIN_BUTTON,
                ToolbarComponentId.ADAPTIVE_BUTTON,
                ToolbarComponentId.RELOAD,
                ToolbarComponentId.FORWARD,
                ToolbarComponentId.HOME,
                ToolbarComponentId.EXTENSIONS_MENU_BUTTON,
                ToolbarComponentId.EXTENSIONS_REQUEST_ACCESS_BUTTON,
                ToolbarComponentId.EXTENSION_ACTION_LIST,
                ToolbarComponentId.OMNIBOX_BOOKMARK,
                ToolbarComponentId.OMNIBOX_CHIP_COLLAPSED,
                ToolbarComponentId.OMNIBOX_ZOOM,
                ToolbarComponentId.OMNIBOX_INSTALL,
                ToolbarComponentId.OMNIBOX_MIC,
                ToolbarComponentId.OMNIBOX_LENS,
                ToolbarComponentId.OMNIBOX_CHIP_EXPANDED
            };

    public static final @ToolbarComponentId int[] APP_MENU_ICON_ROW_COMPONENTS =
            new int[] {
                ToolbarComponentId.RELOAD,
                // TODO(crbug.com/493306650): Revisit after finishing back button experiment.
                ToolbarComponentId.BACK,
                ToolbarComponentId.FORWARD,
                ToolbarComponentId.OMNIBOX_BOOKMARK
            };

    // LINT.IfChange(toolbar_tablet_components)

    @IntDef({
        ToolbarComponentId.HOME,
        ToolbarComponentId.BACK,
        ToolbarComponentId.FORWARD,
        ToolbarComponentId.RELOAD,
        ToolbarComponentId.LOCATION_BAR_MINIMUM,
        ToolbarComponentId.OMNIBOX_CHIP_COLLAPSED,
        ToolbarComponentId.OMNIBOX_CHIP_EXPANDED,
        ToolbarComponentId.OMNIBOX_BOOKMARK,
        ToolbarComponentId.OMNIBOX_ZOOM,
        ToolbarComponentId.OMNIBOX_INSTALL,
        ToolbarComponentId.OMNIBOX_MIC,
        ToolbarComponentId.OMNIBOX_LENS,
        ToolbarComponentId.ADAPTIVE_BUTTON,
        ToolbarComponentId.INCOGNITO_INDICATOR,
        ToolbarComponentId.POPPED_EXTENSION_ACTION,
        ToolbarComponentId.EXTENSIONS_MENU_BUTTON,
        ToolbarComponentId.EXTENSIONS_REQUEST_ACCESS_BUTTON,
        ToolbarComponentId.EXTENSION_ACTION_LIST,
        ToolbarComponentId.TAB_SWITCHER,
        ToolbarComponentId.MENU,
        ToolbarComponentId.PADDING,
        ToolbarComponentId.SIGNIN_BUTTON,
        ToolbarComponentId.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ToolbarComponentId {
        int HOME = 0;
        int BACK = 1;
        int FORWARD = 2;
        int RELOAD = 3;
        int LOCATION_BAR_MINIMUM = 4;
        int OMNIBOX_CHIP_COLLAPSED = 5;
        int OMNIBOX_CHIP_EXPANDED = 6;
        int OMNIBOX_BOOKMARK = 7;
        int OMNIBOX_ZOOM = 8;
        int OMNIBOX_INSTALL = 9;
        int OMNIBOX_MIC = 10;
        int OMNIBOX_LENS = 11;
        int ADAPTIVE_BUTTON = 12;
        int INCOGNITO_INDICATOR = 13;
        int POPPED_EXTENSION_ACTION = 14;
        int EXTENSIONS_MENU_BUTTON = 15;
        int EXTENSION_ACTION_LIST = 16;
        int TAB_SWITCHER = 17;
        int MENU = 18;
        int PADDING = 19;
        int SIGNIN_BUTTON = 20;
        int EXTENSIONS_REQUEST_ACCESS_BUTTON = 21;
        int COUNT = 22;
    }

    // LINT.ThenChange(//chrome/browser/ui/android/toolbar/java/res/layout/toolbar_tablet.xml:toolbar_tablet_components|//chrome/browser/ui/android/omnibox/java/res/layout/url_action_container.xml:toolbar_tablet_components)

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
