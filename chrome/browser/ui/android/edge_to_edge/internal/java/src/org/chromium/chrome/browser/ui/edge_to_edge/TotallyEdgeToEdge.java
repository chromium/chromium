// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Explores further ideas to activate EdgeToEdge in various circumstances. */
public class TotallyEdgeToEdge {
    private static final String TAG = "E2E_TotallyE2E";
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private boolean mShouldDrawEdgeToEdge;
    private BrowserControlsStateProvider.Observer mObserver;
    private Runnable mRunnable;

    /**
     * Creates a controller to enable Edge To Edge under conditions worth exploring.
     *
     * @param browserControlsStateProvider Provider for the Browser Controls (Toolbar) state.
     * @param runnable A callback to activate when we should switch ToEdge or ToNormal.
     */
    public TotallyEdgeToEdge(
            BrowserControlsStateProvider browserControlsStateProvider, Runnable runnable) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate) {
                        Log.v(TAG, "topOffset changed to %s", topOffset);
                        mShouldDrawEdgeToEdge = topOffset != 0;
                        mRunnable.run();
                    }
                };
        browserControlsStateProvider.addObserver(mObserver);
        mRunnable = runnable;
    }

    /**
     * @return Whether this Feature is enabled or not.
     */
    static boolean isEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.TOTALLY_EDGE_TO_EDGE);
    }

    /**
     * @return whether this class thinks we should draw ToEdge now, or not.
     */
    boolean shouldDrawToEdge() {
        return mShouldDrawEdgeToEdge;
    }

    /** Call upon destruction for cleanup. */
    void destroy() {
        if (mObserver != null) {
            mBrowserControlsStateProvider.removeObserver(mObserver);
            mObserver = null;
        }
    }
}
