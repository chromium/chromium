// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import android.util.DisplayMetrics;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
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
        var delegate = mTabStripTransitionDelegateSupplier.get();
        // Skip while the delegate is null before native init; this method will be invoked by the
        // observer callback once the delegate supplier is injected.
        if (delegate == null) return;
        mTabStripTransitionThreshold =
                ViewUtils.dpToPx(displayMetrics, delegate.getFadeTransitionThresholdDp());
    }

    void onTabStripSizeChanged(
            int width, boolean forceFadeInStrip, boolean desktopWindowingModeChanged) {
        if (width == mTabStripWidth && !desktopWindowingModeChanged) return;
        mTabStripWidth = width;
        requestTransition(forceFadeInStrip);
    }

    private void requestTransition(boolean forceFadeInStrip) {
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
