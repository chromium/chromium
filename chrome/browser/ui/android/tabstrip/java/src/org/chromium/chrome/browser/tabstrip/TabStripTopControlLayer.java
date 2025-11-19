// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabstrip;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionHandler;

/**
 * Top control layer representing tab strip. It can have different state than the current height
 * store in the StripLayoutHelperManager, as this represents the target height it is going for
 * during tab strip height transition.
 */
@NullMarked
public class TabStripTopControlLayer extends ObservableSupplierImpl<Integer>
        implements TopControlLayer, TabStripTransitionHandler {
    private static final String TAG = "TabStripLayer";
    private final TopControlsStacker mTopControlsStacker;

    private @Nullable BrowserControlsOffsetTagsInfo mOffsetTagsInfo;

    // Not null after #initializeWithNative.
    private @MonotonicNonNull TabStripSceneLayerHolder mTabStrip;

    // Not null after transition started.
    private @Nullable TransitionState mTransitionState;

    /** Class to to provide transition state information. */
    static class TransitionState {
        public final int startHeight;
        public final int targetHeight;
        public final boolean applyScrimOverlay;
        public final @TopControlVisibility int visibility;

        private boolean mIsStarted;

        private TransitionState(int startHeight, int targetHeight, boolean applyScrimOverlay) {
            this.startHeight = startHeight;
            this.targetHeight = targetHeight;
            this.applyScrimOverlay = applyScrimOverlay;

            visibility = calculateVisibility(startHeight, targetHeight);
        }

        private static @TopControlVisibility int calculateVisibility(
                int startHeight, int targetHeight) {
            if (startHeight == targetHeight) {
                return targetHeight > 0
                        ? TopControlVisibility.VISIBLE
                        : TopControlVisibility.HIDDEN;
            }

            boolean isIncreasing = startHeight < targetHeight;
            return isIncreasing
                    ? TopControlVisibility.SHOWING_TOP_ANCHOR
                    : TopControlVisibility.HIDING_TOP_ANCHOR;
        }

        /** Returns true only when this method is called the first time. */
        public boolean checkIsFirstUpdate() {
            if (mIsStarted) return false;

            mIsStarted = true;
            return true;
        }
    }

    /**
     * Create the layer representation for the tab strip in the browser controls.
     *
     * @param tabStripHeight The initial height of the tab strip.
     * @param topControlsStacker The top controls stacker instance.
     */
    public TabStripTopControlLayer(int tabStripHeight, TopControlsStacker topControlsStacker) {
        super(tabStripHeight);
        mTopControlsStacker = topControlsStacker;

        if (ChromeFeatureList.sTopControlsRefactor.isEnabled()) {
            mTopControlsStacker.addControl(this);
        }
    }

    /** Destroy the instance and remove all dependencies. */
    public void destroy() {
        mTopControlsStacker.removeControl(this);
    }

    /**
     * Add the post-native dependencies.
     *
     * @param tabStrip {@link StripLayoutHelperManager} instance that presents the tab strip.
     */
    public void initializeWithNative(TabStripSceneLayerHolder tabStrip) {
        mTabStrip = tabStrip;
    }

    // Implements TopControlLayer

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.TABSTRIP;
    }

    @Override
    public int getTopControlHeight() {
        return get();
    }

    @Override
    public int getTopControlVisibility() {
        if (BrowserControlsUtils.isTopControlsRefactorOffsetEnabled() && mTransitionState != null) {
            return mTransitionState.visibility;
        }

        // The tab strip adds to the total height of the top controls regardless of whether or
        // not it is "visible" to the user, i.e. we take its inherent height into account even
        // when scrolled offscreen or obscured, except when hidden by height transition.
        //
        // TODO(crbug.com/417238089): Possibly add way to notify stacker of visibility changes.
        boolean isTabStripVisibleAsLayer = get() > 0;
        return isTabStripVisibleAsLayer
                ? TopControlVisibility.VISIBLE
                : TopControlVisibility.HIDDEN;
    }

    @Override
    public void updateOffsetTag(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {
        if (mOffsetTagsInfo == offsetTagsInfo) return;

        mOffsetTagsInfo = offsetTagsInfo;
        if (mTabStrip != null && !isInTransition()) {
            mTabStrip.updateOffsetTagsInfo(offsetTagsInfo);
        }
    }

    @Override
    public void onAndroidControlsVisibilityChanged(int visibility) {
        if (isInTransition() && mTransitionState.checkIsFirstUpdate()) {
            handleTransitionStart();
        }
    }

    @Override
    public void onBrowserControlsOffsetUpdate(int layerYOffset, boolean reachRestingPosition) {
        if (!isInTransition()) {
            updateSceneLayerOffset(layerYOffset);
            return;
        }

        if (mTransitionState.checkIsFirstUpdate()) {
            handleTransitionStart();
        }
        updateSceneLayerOffset(layerYOffset);
        if (reachRestingPosition) {
            handleTransitionFinished();
        }
    }

    // Implements TabStripTransitionHandler

    @Override
    public void onTransitionRequested(int newHeight, boolean applyScrimOverlay) {
        prepForTransitionRequested(newHeight, applyScrimOverlay);

        // TODO(crbug.com/41481630): Supplier can have an inconsistent value with
        //  mToolbar.getTabStripHeight().
        set(newHeight);
    }

    private void prepForTransitionRequested(int newHeight, boolean applyScrimOverlay) {
        if (mTabStrip == null) return;

        if (mTransitionState != null) {
            notifyTransitionFinished(false);
        }
        mTransitionState = new TransitionState(getTopControlHeight(), newHeight, applyScrimOverlay);
        mTabStrip.updateOffsetTagsInfo(null);
    }

    @EnsuresNonNullIf("mTransitionState")
    private boolean isInTransition() {
        return mTransitionState != null;
    }

    private void handleTransitionStart() {
        if (!isInTransition()) return;
        notifyTransitionStarted();
    }

    private void handleTransitionFinished() {
        if (!isInTransition()) return;

        // Once transition is finished, put the offset tags back so layers can scroll as intended.
        assertNonNull(mTabStrip).updateOffsetTagsInfo(mOffsetTagsInfo);

        notifyTransitionFinished(true);
        mTransitionState = null;
    }

    private void updateSceneLayerOffset(int yOffset) {
        if (mTabStrip == null) return;
        mTabStrip.onLayerYOffsetChanged(yOffset);
    }

    private void notifyTransitionStarted() {
        assertNonNull(mTransitionState);
        assertNonNull(mTabStrip)
                .onHeightChanged(mTransitionState.targetHeight, mTransitionState.applyScrimOverlay);
    }

    private void notifyTransitionFinished(boolean finished) {
        assertNonNull(mTabStrip).onHeightTransitionFinished();
        if (!finished) {
            Log.i(TAG, "Transition canceled.");
        }
    }
}
