// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Explores further ideas to activate EdgeToEdge in various circumstances. */
public class TotallyEdgeToEdge {
    /** Adjusts edges for Edge To Edge to a given suggested padding fraction. */
    interface EdgeAdjustor {
        /** Suggests an adjustment of drawing to edges to the suggested fraction (0-1.0). */
        void adjustEdges(float suggestedPadding);
    }

    private static final String TAG = "E2E_TotallyE2E";
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private boolean mShouldDrawEdgeToEdge;
    private BrowserControlsStateProvider.Observer mObserver;
    private EdgeAdjustor mEdgeAdjustor;

    /**
     * Creates a controller to enable Edge To Edge under conditions worth exploring.
     *
     * @param browserControlsStateProvider Provider for the Browser Controls (Toolbar) state.
     * @param edgeAdjustor A callback to activate when we should adjust ToEdge or ToNormal.
     */
    public TotallyEdgeToEdge(
            BrowserControlsStateProvider browserControlsStateProvider, EdgeAdjustor edgeAdjustor) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mObserver =
                new BrowserControlsStateProvider.Observer() {
                    private int mMax;

                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate) {
                        if (Math.abs(topOffset) > mMax) mMax = Math.abs(topOffset);
                        mShouldDrawEdgeToEdge = topOffset != 0;
                        mEdgeAdjustor.adjustEdges((Math.abs((float) topOffset) / mMax));
                    }
                };
        browserControlsStateProvider.addObserver(mObserver);
        mEdgeAdjustor = edgeAdjustor;
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
