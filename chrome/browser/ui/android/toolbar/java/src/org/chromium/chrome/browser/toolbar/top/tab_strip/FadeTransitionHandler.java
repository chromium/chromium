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
    private boolean mTabStripSuppressed;

    FadeTransitionHandler(
            OneshotSupplier<TabStripTransitionDelegate> tabStripTransitionDelegateSupplier,
            CallbackController callbackController) {
        mTabStripTransitionDelegateSupplier = tabStripTransitionDelegateSupplier;
        mCallbackController = callbackController;
    }

    void updateTabStripTransitionThreshold(
            DisplayMetrics displayMetrics, int tabStripWidth, boolean isInDesktopWindow) {
        var delegate = mTabStripTransitionDelegateSupplier.get();
        // Skip while the delegate is null before native init; this method will be invoked by the
        // observer callback once the delegate supplier is injected.
        if (delegate == null) return;
        mTabStripTransitionThreshold =
                ViewUtils.dpToPx(displayMetrics, delegate.getFadeTransitionThresholdDp());
        mTabStripWidth = tabStripWidth;

        // Fade transition should update strip visibility when the threshold changes in desktop
        // windowing mode, after re-evaluating the current tab strip width. In the exceptional
        // scenario where the strip needs to be made forcefully visible via a fade transition,
        // we expect a tab strip size change to trigger the transition.
        if (isInDesktopWindow) {
            requestTransition(/* forceFadeInStrip= */ false);
        }
    }

    void onTabStripSizeChanged(
            int tabStripWidth, boolean forceFadeInStrip, boolean desktopWindowingModeChanged) {
        // We bypass the width early-return check when forceFadeInStrip is true. This ensures
        // that if the width was already updated by a prior non-transition call (such as
        // updateTabStripTransitionThreshold() or suppressTabStrip()), we do not accidentally
        // drop or skip executing the forceful transition.
        if (tabStripWidth == mTabStripWidth && !desktopWindowingModeChanged && !forceFadeInStrip) {
            return;
        }
        mTabStripWidth = tabStripWidth;
        requestTransition(forceFadeInStrip);
    }

    void suppressTabStrip(boolean suppress, int tabStripWidth, boolean isInDesktopWindow) {
        if (mTabStripSuppressed == suppress) return;
        mTabStripSuppressed = suppress;
        mTabStripWidth = tabStripWidth;

        if (isInDesktopWindow) {
            requestTransition(/* forceFadeInStrip= */ false);
        }
    }

    private void requestTransition(boolean forceFadeInStrip) {
        mTabStripTransitionDelegateSupplier.runSyncOrOnAvailable(
                mCallbackController.makeCancelable(
                        delegate -> maybeUpdateTabStripVisibility(forceFadeInStrip)));
    }

    private void maybeUpdateTabStripVisibility(boolean forceFadeInStrip) {
        if (mTabStripWidth <= 0) return;

        boolean showTabStrip =
                (mTabStripWidth >= mTabStripTransitionThreshold || forceFadeInStrip)
                        && !mTabStripSuppressed;
        var newOpacity = showTabStrip ? 0f : 1f;

        var delegate = mTabStripTransitionDelegateSupplier.get();
        assert delegate != null : "TabStripTransitionDelegate should be available.";

        delegate.onFadeTransitionRequested(newOpacity, FADE_TRANSITION_DURATION_MS);
    }
}
