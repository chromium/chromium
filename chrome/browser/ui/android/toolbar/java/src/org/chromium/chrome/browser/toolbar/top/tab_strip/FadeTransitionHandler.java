// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import android.util.DisplayMetrics;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.ui.base.ViewUtils;

/**
 * Owned and used by {@link TabStripTransitionCoordinator} to manage showing / hiding the tab strip
 * by an in-place fade transition facilitated by a scrim update.
 */
@NullMarked
class FadeTransitionHandler {
    private static final int FADE_TRANSITION_DURATION_MS = 200;

    private final TabStripTransitionDelegate mTabStripTransitionDelegate;

    private int mTabStripTransitionThreshold;
    private int mTabStripWidth;

    FadeTransitionHandler(
            TabStripTransitionDelegate tabStripTransitionDelegate,
            CallbackController callbackController) {
        mTabStripTransitionDelegate = tabStripTransitionDelegate;
    }

    void updateTabStripTransitionThreshold(DisplayMetrics displayMetrics) {
        mTabStripTransitionThreshold =
                ViewUtils.dpToPx(
                        displayMetrics, mTabStripTransitionDelegate.getFadeTransitionThresholdDp());
    }

    void onTabStripSizeChanged(
            int width, boolean forceFadeInStrip, boolean desktopWindowingModeChanged) {
        if (width == mTabStripWidth && !desktopWindowingModeChanged) return;
        mTabStripWidth = width;
        requestTransition(forceFadeInStrip);
    }

    private void requestTransition(boolean forceFadeInStrip) {
        maybeUpdateTabStripVisibility(forceFadeInStrip);
    }

    private void maybeUpdateTabStripVisibility(boolean forceFadeInStrip) {
        if (mTabStripWidth <= 0) return;

        boolean showTabStrip = mTabStripWidth >= mTabStripTransitionThreshold || forceFadeInStrip;
        var newOpacity = showTabStrip ? 0f : 1f;

        mTabStripTransitionDelegate.onFadeTransitionRequested(
                newOpacity, FADE_TRANSITION_DURATION_MS);
    }
}
