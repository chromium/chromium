// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabstrip;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionHandler;
import org.chromium.ui.util.TokenHolder;

/**
 * Top control layer representing tab strip. It can have different state than the current height
 * store in the StripLayoutHelperManager, as this represents the target height it is going for
 * during tab strip height transition.
 */
@NullMarked
public class TabStripTopControlLayer implements TopControlLayer, TabStripTransitionHandler {
    private static final String TAG = "TabStripLayer";
    private final TopControlsStacker mTopControlsStacker;
    private final BrowserControlsStateProvider mBrowserControls;
    private final ControlContainer mControlContainer;
    private final SettableNonNullObservableSupplier<Integer> mSupplier;
    private final @Nullable TokenHolder mLockTopControlsTokenJar;

    private int mLockTopControlsToken = TokenHolder.INVALID_TOKEN;
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
        public final Runnable transitionStartedCallback;
        public final boolean hasAnimation;
        public final @TopControlVisibility int visibility;

        private boolean mIsStarted;

        private TransitionState(
                int startHeight,
                int targetHeight,
                boolean applyScrimOverlay,
                Runnable transitionStartedCallback) {
            this.startHeight = startHeight;
            this.targetHeight = targetHeight;
            this.applyScrimOverlay = applyScrimOverlay;
            this.transitionStartedCallback = transitionStartedCallback;

            hasAnimation = calculateHasAnimation(startHeight, targetHeight, applyScrimOverlay);
            visibility = calculateVisibility(startHeight, targetHeight, hasAnimation);
        }

        private static @TopControlVisibility int calculateVisibility(
                int startHeight, int targetHeight, boolean hasAnimation) {
            if (!hasAnimation) {
                return targetHeight > 0
                        ? TopControlVisibility.VISIBLE
                        : TopControlVisibility.HIDDEN;
            }

            boolean isIncreasing = startHeight < targetHeight;
            return isIncreasing
                    ? TopControlVisibility.SHOWING_TOP_ANCHOR
                    : TopControlVisibility.HIDING_TOP_ANCHOR;
        }

        private static boolean calculateHasAnimation(
                int startHeight, int targetHeight, boolean applyScrimOverlay) {
            if (startHeight == targetHeight) return false;

            return applyScrimOverlay && (startHeight == 0 || targetHeight == 0);
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
     * @param browserControls The browser controls instance.
     * @param controlContainer The {@link ControlContainer} instance.
     * @param lockTopControlsTokenJar {@link TokenHolder} from TopControlLockCoordinator that used
     *     to preserve the lock top controls state.
     */
    public TabStripTopControlLayer(
            int tabStripHeight,
            TopControlsStacker topControlsStacker,
            BrowserControlsStateProvider browserControls,
            ControlContainer controlContainer,
            @Nullable TokenHolder lockTopControlsTokenJar) {
        mTopControlsStacker = topControlsStacker;
        mBrowserControls = browserControls;
        mControlContainer = controlContainer;
        mLockTopControlsTokenJar = lockTopControlsTokenJar;
        mSupplier = ObservableSuppliers.createNonNull(tabStripHeight);

        if (ChromeFeatureList.sTopControlsRefactor.isEnabled()) {
            mTopControlsStacker.addControl(this);
        }
    }

    /** Destroy the instance and remove all dependencies. */
    public void destroy() {
        mTopControlsStacker.removeControl(this);
        mSupplier.destroy();
        if (mLockTopControlsTokenJar != null) {
            mLockTopControlsTokenJar.releaseToken(mLockTopControlsToken);
            mLockTopControlsToken = TokenHolder.INVALID_TOKEN;
        }
    }

    /**
     * Add the post-native dependencies.
     *
     * @param tabStrip {@link StripLayoutHelperManager} instance that presents the tab strip.
     */
    public void initializeWithNative(TabStripSceneLayerHolder tabStrip) {
        mTabStrip = tabStrip;

        mTabStrip.updateOffsetTagsInfo(mOffsetTagsInfo);
    }

    public NonNullObservableSupplier<Integer> getSupplier() {
        return mSupplier;
    }

    public void set(int tabStripHeight) {
        mSupplier.set(tabStripHeight);
    }

    // Implements TopControlLayer

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.TABSTRIP;
    }

    @Override
    public int getTopControlHeight() {
        return mSupplier.get();
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
        boolean isTabStripVisibleAsLayer = getTopControlHeight() > 0;
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

    @Override
    public void prepForHeightAdjustmentAnimation(int latestYOffset) {
        if (mTabStrip == null) return;
        // This is called after onTransitionRequested, which triggers the change in visibility.
        // The offset tags needs to be set here, since the tab strip do not want to drive the
        // animation using the offset tags.
        mTabStrip.updateOffsetTagsInfo(null);
        mTabStrip.onLayerYOffsetChanged(latestYOffset, getTopControlHeight());
    }

    // Implements TabStripTransitionHandler

    @Override
    public void onTransitionRequested(
            int newHeight, boolean applyScrimOverlay, Runnable transitionStartedCallback) {
        prepForTransitionRequested(newHeight, applyScrimOverlay, transitionStartedCallback);

        // TODO(crbug.com/41481630): Supplier can have an inconsistent value with
        //  mToolbar.getTabStripHeight().
        mSupplier.set(newHeight);

        if (BrowserControlsUtils.isTopControlsRefactorOffsetEnabled()
                && isInTransition()
                && mTransitionState.targetHeight != mTransitionState.startHeight) {
            mTopControlsStacker.requestLayerUpdateSync(mTransitionState.hasAnimation);
        }
    }

    private void prepForTransitionRequested(
            int newHeight, boolean applyScrimOverlay, Runnable onHeightTransitionStartCallback) {
        if (mTabStrip == null && !canTransitionWithoutTabStrip()) return;

        if (mTransitionState != null) {
            notifyTransitionFinished(false);
        }

        // Request lock top controls token before we establish the transition, since it might
        // trigger a call to update the browser controls height.
        if (mLockTopControlsTokenJar != null) {
            int oldToken = mLockTopControlsToken;
            mLockTopControlsToken = mLockTopControlsTokenJar.acquireToken();
            mLockTopControlsTokenJar.releaseToken(oldToken);
        }

        mTransitionState =
                new TransitionState(
                        getTopControlHeight(),
                        newHeight,
                        applyScrimOverlay,
                        onHeightTransitionStartCallback);
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
        if (mTabStrip != null) {
            mTabStrip.updateOffsetTagsInfo(mOffsetTagsInfo);
        } else {
            assert canTransitionWithoutTabStrip() : "Transition started when mTabStrip == null.";
        }

        if (mLockTopControlsTokenJar != null) {
            mLockTopControlsTokenJar.releaseToken(mLockTopControlsToken);
            mLockTopControlsToken = TokenHolder.INVALID_TOKEN;
        }

        notifyTransitionFinished(true);
        mTransitionState = null;

        // Post an update request, so that the layers can get an update and have the chance populate
        // their resting state (e.g. yOffset, offset tags, etc.)
        mTopControlsStacker.requestLayerUpdatePost(/* requireAnimate= */ false);
    }

    private void updateSceneLayerOffset(int yOffset) {
        if (mTabStrip == null) return;

        assert getTopControlVisibility() == TopControlVisibility.HIDDEN
                || mTopControlsStacker.isLayerAtTop(TopControlType.TABSTRIP);

        int effectiveHeight = getTopControlHeight();
        if (isInTransition()) {
            assert mBrowserControls.getBottomControlsMinHeightOffset() == 0;

            // Assuming tab strip is the top most layer, and there's no minHeight available.
            // The top controls offset stands for the folded portion of the tab strip when negative
            // (tab strip showing), or remaining portion when offset > 0 (tab strip hiding).
            int topControlOffset = mBrowserControls.getTopControlOffset();
            if (topControlOffset <= 0) {
                effectiveHeight += topControlOffset;
            } else {
                effectiveHeight = topControlOffset;
            }
        }
        mTabStrip.onLayerYOffsetChanged(yOffset, effectiveHeight);
    }

    private void notifyTransitionStarted() {
        assertNonNull(mTransitionState);
        mTransitionState.transitionStartedCallback.run();
        mControlContainer.onHeightChanged(
                mTransitionState.targetHeight, mTransitionState.applyScrimOverlay);
        if (mTabStrip != null) {
            mTabStrip.onHeightChanged(
                    mTransitionState.targetHeight, mTransitionState.applyScrimOverlay);
        } else {
            assert canTransitionWithoutTabStrip() : "Transition started when mTabStrip == null.";
        }
    }

    private void notifyTransitionFinished(boolean success) {
        mControlContainer.onHeightTransitionFinished(success);
        if (mTabStrip != null) {
            mTabStrip.onHeightTransitionFinished(success);
        } else {
            assert canTransitionWithoutTabStrip() : "Transition finished when mTabStrip == null.";
        }

        recordTabStripTransitionFinished(success);
        if (!success) {
            Log.i(TAG, "Transition canceled.");
        }
    }

    private void recordTabStripTransitionFinished(boolean finished) {
        RecordHistogram.recordBooleanHistogram(
                "Android.DynamicTopChrome.TabStripTransition.Finished", finished);
    }

    private boolean canTransitionWithoutTabStrip() {
        return BrowserControlsUtils.isForceTopChromeHeightAdjustmentOnStartupEnabled(
                mControlContainer.getView().getContext());
    }
}
