// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import android.util.DisplayMetrics;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.ui.base.ViewUtils;

/**
 * Owned and used by {@link TabStripTransitionCoordinator} to manage showing / hiding the tab strip
 * by an in-place fade transition facilitated by a scrim update.
 */
@NullMarked
class FadeTransitionHandler {
    // Minimum width (in dp) of the tab strip for it to be shown.
    // 284 = 2 * minTabWidth(108) - tabOverlap(28) + newTabButton (48) + modelSelectorButton(48).
    static final int TRANSITION_THRESHOLD_DP = 284;
    private static final int FADE_TRANSITION_DURATION_MS = 200;

    private final OneshotSupplier<TabStripTransitionDelegate> mTabStripTransitionDelegateSupplier;
    private final CallbackController mCallbackController;

    private int mTabStripTransitionThreshold;
    private int mTabStripWidth;

    FadeTransitionHandler(
            OneshotSupplier<TabStripTransitionDelegate> tabStripTransitionDelegateSupplier,
            CallbackController callbackController) {
        mTabStripTransitionDelegateSupplier = tabStripTransitionDelegateSupplier;
        mCallbackController = callbackController;
    }

    void updateTabStripTransitionThreshold(DisplayMetrics displayMetrics) {
        mTabStripTransitionThreshold = ViewUtils.dpToPx(displayMetrics, TRANSITION_THRESHOLD_DP);
    }

    void onTabStripSizeChanged(
            int width, boolean forceFadeInStrip, boolean desktopWindowingModeChanged) {
        if (width == mTabStripWidth && !desktopWindowingModeChanged) return;
        mTabStripWidth = width;
        requestTransition(forceFadeInStrip);
    }

    private void requestTransition(boolean forceFadeInStrip) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.TAB_STRIP_TRANSITION_IN_DESKTOP_WINDOW)) {
            return;
        }
        mTabStripTransitionDelegateSupplier.runSyncOrOnAvailable(
                mCallbackController.makeCancelable(
                        delegate -> maybeUpdateTabStripVisibility(forceFadeInStrip)));
    }

    private void maybeUpdateTabStripVisibility(boolean forceFadeInStrip) {
        if (mTabStripWidth <= 0) return;

        boolean showTabStrip = mTabStripWidth >= mTabStripTransitionThreshold || forceFadeInStrip;
        var newOpacity = showTabStrip ? 0f : 1f;

        var delegate = mTabStripTransitionDelegateSupplier.get();
        assert delegate != null : "TabStripTransitionDelegate should be available.";

        delegate.onFadeTransitionRequested(newOpacity, FADE_TRANSITION_DURATION_MS);
    }
}
