// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import android.util.DisplayMetrics;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.ui.base.ViewUtils;

/**
 * Owned and used by {@link TabStripTransitionCoordinator} to manage showing / hiding the tab strip
 * by an in-place fade transition facilitated by a scrim update.
 */
class FadeTransitionHandler {
    // Minimum width (in dp) of the tab strip for it to be shown.
    // 284 = 2 * minTabWidth(108) - tabOverlap(28) + newTabButton (48) + modelSelectorButton(48).
    private static final int TRANSITION_THRESHOLD_DP = 284;
    private static final int FADE_TRANSITION_DURATION_MS = 200;

    private final OneshotSupplier<TabStripTransitionDelegate> mTabStripTransitionDelegateSupplier;
    private final CallbackController mCallbackController;

    private int mTabStripTransitionThreshold;
    private int mTabStripWidth;
    // The current opacity of the tab strip scrim layer.
    private float mScrimOpacity;

    FadeTransitionHandler(
            OneshotSupplier<TabStripTransitionDelegate> tabStripTransitionDelegateSupplier,
            CallbackController callbackController) {
        mTabStripTransitionDelegateSupplier = tabStripTransitionDelegateSupplier;
        mCallbackController = callbackController;
    }

    void setTabStripSize(int width) {
        mTabStripWidth = width;
    }

    void updateTabStripTransitionThreshold(DisplayMetrics displayMetrics) {
        mTabStripTransitionThreshold = ViewUtils.dpToPx(displayMetrics, getStripWidthThresholdDp());

        // TODO (crbug/342641516): Find an alternate way to trigger #requestTransition in
        // instrumentation testing.
        if (TabStripTransitionCoordinator.sFadeTransitionThresholdForTesting != null) {
            requestTransition();
        }
    }

    void requestTransition() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.TAB_STRIP_TRANSITION_IN_DESKTOP_WINDOW)) {
            return;
        }
        mTabStripTransitionDelegateSupplier.runSyncOrOnAvailable(
                mCallbackController.makeCancelable(delegate -> maybeUpdateTabStripVisibility()));
    }

    private void maybeUpdateTabStripVisibility() {
        if (mTabStripWidth <= 0) return;

        boolean showTabStrip = mTabStripWidth >= mTabStripTransitionThreshold;

        var newOpacity = showTabStrip ? 0f : 1f;
        if (newOpacity == mScrimOpacity) return;

        var delegate = mTabStripTransitionDelegateSupplier.get();
        assert delegate != null : "TabStripTransitionDelegate should be available.";

        delegate.onFadeTransitionRequested(mScrimOpacity, newOpacity, FADE_TRANSITION_DURATION_MS);
        mScrimOpacity = newOpacity;
    }

    /** Get the min strip width (in dp) required for it to become visible. */
    private static int getStripWidthThresholdDp() {
        if (TabStripTransitionCoordinator.sFadeTransitionThresholdForTesting != null) {
            return TabStripTransitionCoordinator.sFadeTransitionThresholdForTesting;
        }
        return TRANSITION_THRESHOLD_DP;
    }
}
