// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

/** The business logic for controlling the top toolbar's cc texture. */
@NullMarked
public class TopToolbarOverlayMediator {
    // Forced testing params.
    private static @Nullable Boolean sIsTabletForTesting;
    private static @Nullable Integer sToolbarBackgroundColorForTesting;
    private static @Nullable Integer sUrlBarColorForTesting;

    /** An Android Context. */
    private final Context mContext;

    /** A handle to the layout manager for observing scene changes. */
    private final LayoutStateProvider mLayoutStateProvider;

    /** The observer of changes to the active layout. */
    private final LayoutStateObserver mSceneChangeObserver;

    /** A means of populating draw info for the progress bar. */
    private final Callback<ClipDrawableProgressBar.DrawingInfo> mProgressInfoCallback;

    /** An observer that watches for changes in the active tab. */
    private final CurrentTabObserver mTabObserver;

    /** Access to the current state of the browser controls. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** An observer of the browser controls offsets. */
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    private final TopUiThemeColorProvider mTopUiThemeColorProvider;

    /** The view state for this overlay. */
    private final PropertyModel mModel;

    private final ObservableSupplier<Integer> mBottomToolbarControlsOffsetSupplier;
    private final ObservableSupplier<Boolean> mSuppressToolbarSceneLayerSupplier;

    /** Whether visibility is controlled internally or manually by the feature. */
    private boolean mIsVisibilityManuallyControlled;

    /** Whether the android view for this overlay is visible. */
    private boolean mIsToolbarAndroidViewVisible;

    /** Whether the parent of the view for this overlay is visible. */
    private boolean mIsBrowserControlsAndroidViewVisible;

    /** Whether the overlay should be visible despite other signals. */
    private boolean mManualVisibility;

    /** Whether a layout that this overlay can be displayed on is showing. */
    private boolean mIsOnValidLayout;

    private final ObservableSupplier<@Nullable Tab> mTabSupplier;
    private float mViewportHeight;

    private @Nullable OffsetTag mTopControlsOffsetTag;
    private @Nullable OffsetTag mBottomControlsOffsetTag;
    private @ControlsPosition int mControlsPosition;

    TopToolbarOverlayMediator(
            PropertyModel model,
            Context context,
            LayoutStateProvider layoutStateProvider,
            Callback<ClipDrawableProgressBar.DrawingInfo> progressInfoCallback,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            TopUiThemeColorProvider topUiThemeColorProvider,
            ObservableSupplier<Integer> bottomToolbarControlsOffsetSupplier,
            ObservableSupplier<Boolean> suppressToolbarSceneLayerSupplier,
            int layoutsToShowOn,
            boolean manualVisibilityControl) {
        mContext = context;
        mLayoutStateProvider = layoutStateProvider;
        mProgressInfoCallback = progressInfoCallback;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mModel = model;
        mBottomToolbarControlsOffsetSupplier = bottomToolbarControlsOffsetSupplier;
        mSuppressToolbarSceneLayerSupplier = suppressToolbarSceneLayerSupplier;
        mBottomToolbarControlsOffsetSupplier.addObserver((unused) -> updateContentOffset());
        mSuppressToolbarSceneLayerSupplier.addObserver((suppress) -> updateVisibility());
        mIsVisibilityManuallyControlled = manualVisibilityControl;
        mIsOnValidLayout = (mLayoutStateProvider.getActiveLayoutType() & layoutsToShowOn) > 0;
        mTabSupplier = tabSupplier;
        mControlsPosition = mBrowserControlsStateProvider.getControlsPosition();
        updateVisibility();

        mSceneChangeObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        mIsOnValidLayout = (layoutType & layoutsToShowOn) > 0;
                        updateVisibility();
                    }
                };
        mLayoutStateProvider.addObserver(mSceneChangeObserver);

        // Keep an observer attached to the visible tab (and only the visible tab) to update
        // properties including theme color.
        Callback<@Nullable Tab> activityTabCallback =
                (tab) -> {
                    if (tab == null) return;
                    updateVisibility();
                    updateThemeColor(tab);
                    updateProgress();
                    updateAnonymize(tab);
                };
        mTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onDidChangeThemeColor(Tab tab, int color) {
                                updateThemeColor(tab);
                            }

                            @Override
                            public void onLoadProgressChanged(Tab tab, float progress) {
                                updateProgress();
                            }

                            @Override
                            public void onContentChanged(Tab tab) {
                                updateVisibility();
                                updateThemeColor(tab);
                                updateAnonymize(tab);
                            }

                            @Override
                            public void didBackForwardTransitionAnimationChange(Tab tab) {
                                updateVisibility();
                            }
                        },
                        activityTabCallback);

        activityTabCallback.onResult(tabSupplier.get());
        mTabObserver.triggerWithCurrentTab();

        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            boolean topControlsMinHeightChanged,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean bottomControlsMinHeightChanged,
                            boolean requestNewFrame,
                            boolean isVisibilityForced) {
                        if (!ChromeFeatureList.sBrowserControlsInViz.isEnabled()
                                || requestNewFrame
                                || isVisibilityForced) {
                            updateContentOffset();
                        } else {
                            // We need to set the height, as it would have changed if this is the
                            // first frame of an animation. Any existing offsets from scrolling and
                            // animations will be applied by OffsetTags.
                            int height = mBrowserControlsStateProvider.getTopControlsHeight();
                            if (mControlsPosition == ControlsPosition.TOP) {
                                mModel.set(TopToolbarOverlayProperties.CONTENT_OFFSET, height);
                            } else if (mControlsPosition == ControlsPosition.BOTTOM) {
                                mModel.set(
                                        TopToolbarOverlayProperties.CONTENT_OFFSET,
                                        mBottomToolbarControlsOffsetSupplier.get()
                                                + mViewportHeight);
                            }
                        }

                        // TODO(peilinwang) Clean up this flag and remove the updateVisibility call
                        // when stable experiment is finished.
                        if (!ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
                            updateShadowState();
                            updateVisibility();
                        }
                    }

                    @Override
                    public void onAndroidControlsVisibilityChanged(int visibility) {
                        mIsBrowserControlsAndroidViewVisible = visibility == View.VISIBLE;
                        updateShadowState();
                    }

                    @Override
                    public void onControlsConstraintsChanged(
                            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                            BrowserControlsOffsetTagsInfo offsetTagsInfo,
                            @BrowserControlsState int constraints,
                            boolean shouldUpdateOffsets) {
                        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
                            mTopControlsOffsetTag = offsetTagsInfo.getTopControlsOffsetTag();
                            mBottomControlsOffsetTag = offsetTagsInfo.getBottomControlsOffsetTag();
                            updateOffsetTag();

                            if (shouldUpdateOffsets) {
                                mModel.set(
                                        TopToolbarOverlayProperties.CONTENT_OFFSET,
                                        mBrowserControlsStateProvider.getContentOffset());
                            }
                        }
                    }

                    @Override
                    public void onControlsPositionChanged(int controlsPosition) {
                        mControlsPosition = controlsPosition;
                        if (ChromeFeatureList.sBcivBottomControls.isEnabled()) {
                            updateOffsetTag();
                        }
                    }
                };
        mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
        mIsBrowserControlsAndroidViewVisible =
                mBrowserControlsStateProvider.getAndroidControlsVisibility() == View.VISIBLE;
    }

    private void updateOffsetTag() {
        if (mControlsPosition == ControlsPosition.TOP) {
            mModel.set(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG, mTopControlsOffsetTag);
        } else if (mControlsPosition == ControlsPosition.BOTTOM) {
            mModel.set(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG, mBottomControlsOffsetTag);
        } else {
            mModel.set(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG, null);
        }
    }

    /**
     * Set whether the android view corresponding with this overlay is showing.
     *
     * @param isVisible Whether the android view is visible.
     */
    void setIsAndroidViewVisible(boolean isVisible) {
        mIsToolbarAndroidViewVisible = isVisible;
        updateShadowState();
    }

    /**
     * Compute whether the texture's shadow should be visible. The shadow is visible whenever the
     * android view is not shown.
     */
    private void updateShadowState() {
        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
            // With BCIV enabled, we show the hairline on the composited toolbar by default,
            // and we don't want to update its visibility from the browser, because that incurs a
            // compositor frame.
            return;
        }

        boolean drawControlsAsTexture = !mIsBrowserControlsAndroidViewVisible;
        boolean showShadow =
                drawControlsAsTexture
                        || !mIsToolbarAndroidViewVisible
                        || mIsVisibilityManuallyControlled;

        mModel.set(TopToolbarOverlayProperties.SHOW_SHADOW, showShadow);
    }

    /**
     * Update the colors of the layer based on the specified tab.
     * @param tab The tab to base the colors on.
     */
    private void updateThemeColor(Tab tab) {
        @ColorInt int color = getToolbarBackgroundColor(tab);
        mModel.set(TopToolbarOverlayProperties.TOOLBAR_BACKGROUND_COLOR, color);
        mModel.set(TopToolbarOverlayProperties.URL_BAR_COLOR, getUrlBarBackgroundColor(tab, color));
    }

    /**
     * @param tab The tab to get the background color for.
     * @return The background color.
     */
    private @ColorInt int getToolbarBackgroundColor(Tab tab) {
        if (sToolbarBackgroundColorForTesting != null) return sToolbarBackgroundColorForTesting;
        return mTopUiThemeColorProvider.getSceneLayerBackground(tab);
    }

    /**
     * @param tab The tab to get the background color for.
     * @param backgroundColor The tab's background color.
     * @return The url bar color.
     */
    private @ColorInt int getUrlBarBackgroundColor(Tab tab, @ColorInt int backgroundColor) {
        if (sUrlBarColorForTesting != null) return sUrlBarColorForTesting;
        return ThemeUtils.getTextBoxColorForToolbarBackground(mContext, tab, backgroundColor);
    }

    /** Update the state of the composited progress bar. */
    private void updateProgress() {
        // Tablets have their own version of a progress "spinner".
        if (isTablet()) return;

        if (mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO) == null) {
            mModel.set(
                    TopToolbarOverlayProperties.PROGRESS_BAR_INFO,
                    new ClipDrawableProgressBar.DrawingInfo());
        }

        // Update and set the progress info to trigger an update; the PROGRESS_BAR_INFO
        // property skips the object equality check.
        mProgressInfoCallback.onResult(mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
        mModel.set(
                TopToolbarOverlayProperties.PROGRESS_BAR_INFO,
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
    }

    /** @return Whether this component is in tablet mode. */
    private boolean isTablet() {
        if (sIsTabletForTesting != null) return sIsTabletForTesting;
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    /** Clean up any state and observers. */
    void destroy() {
        mTabObserver.destroy();

        mLayoutStateProvider.removeObserver(mSceneChangeObserver);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
    }

    /** Update the visibility of the overlay. */
    private void updateVisibility() {
        Tab tab = mTabSupplier.get();
        if (mSuppressToolbarSceneLayerSupplier.get()
                || (tab != null && tab.isNativePage() && tab.isDisplayingBackForwardAnimation())) {
            // TODO(crbug.com/365818512): Add a screenshot capture test to cover this case.
            mModel.set(TopToolbarOverlayProperties.VISIBLE, false);
        } else if (mIsVisibilityManuallyControlled) {
            mModel.set(TopToolbarOverlayProperties.VISIBLE, mManualVisibility && mIsOnValidLayout);
        } else {
            // When BCIV is enabled, we want to show the composited view even if the controls are
            // offscreen, because we want to avoid an additional compositor frame when scrolling
            // them back on screen.
            boolean visibility =
                    (ChromeFeatureList.sBrowserControlsInViz.isEnabled()
                                    || !BrowserControlsUtils.areBrowserControlsOffScreen(
                                            mBrowserControlsStateProvider))
                            && mIsOnValidLayout;
            mModel.set(TopToolbarOverlayProperties.VISIBLE, visibility);
        }
    }

    private void updateAnonymize(Tab tab) {
        if (!mIsVisibilityManuallyControlled) {
            boolean isNativePage = tab.isNativePage();
            mModel.set(TopToolbarOverlayProperties.ANONYMIZE, isNativePage);
        }
    }

    /** @return Whether this overlay should be attached to the tree. */
    boolean shouldBeAttachedToTree() {
        return true;
    }

    /** @param xOffset The x offset of the toolbar. */
    void setXOffset(float xOffset) {
        mModel.set(TopToolbarOverlayProperties.X_OFFSET, xOffset);
    }

    /** @param anonymize Whether the URL should be hidden when the layer is rendered. */
    void setAnonymize(boolean anonymize) {
        mModel.set(TopToolbarOverlayProperties.ANONYMIZE, anonymize);
    }

    /** @param visible Whether the overlay and shadow should be visible despite other signals. */
    void setManualVisibility(boolean visible) {
        assert mIsVisibilityManuallyControlled
                : "Manual visibility control was not set for this overlay.";
        mManualVisibility = visible;
        updateShadowState();
        updateVisibility();
    }

    void setVisibilityManuallyControlledForTesting(boolean manuallyControlled) {
        mIsVisibilityManuallyControlled = manuallyControlled;
        updateShadowState();
        updateVisibility();
    }

    void setViewportHeight(float viewportHeight) {
        if (viewportHeight == mViewportHeight) return;
        mViewportHeight = viewportHeight;
        updateContentOffset();
    }

    private void updateContentOffset() {
        // When top-anchored, the content offset is used to position the toolbar
        // layer instead of the top controls offset because the top controls can
        // have a different height than that of just the toolbar, (e.g. when status
        // indicator is visible or tab strip is hidden), and the toolbar should be
        // positioned at the bottom of the top controls regardless of the overall
        // height.
        // When the toolbar is bottom-anchored, the situation is even more ambiguous
        // because the bottom-anchored toolbar can't be assumed to sit at the top or
        // bottom of the bottom controls stack. Instead, we rely on an offset
        // provided to us indirectly via BottomControlsStacker, which controls the
        // position of bottom controls layers.
        int contentOffset = mBrowserControlsStateProvider.getContentOffset();
        // Don't use mControlsPosition here because it will not have been updated if this function
        // gets called in the middle of a change of position.
        if (mControlsPosition == ControlsPosition.BOTTOM) {
            contentOffset = (int) (mBottomToolbarControlsOffsetSupplier.get() + mViewportHeight);
        } else {
            // If BCIV is enabled, we keep the composited view visible even when hiding the toolbar,
            // but the shadow isn't included in the toolbar's height, so we shift the toolbar up by
            // the shadow's height to hide the toolbar completely.
            if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()
                    && contentOffset == mBrowserControlsStateProvider.getTopControlsMinHeight()) {
                contentOffset -= mBrowserControlsStateProvider.getTopControlsHairlineHeight();
            }
        }

        mModel.set(TopToolbarOverlayProperties.CONTENT_OFFSET, contentOffset);
    }

    static void setIsTabletForTesting(Boolean isTablet) {
        sIsTabletForTesting = isTablet;
        ResettersForTesting.register(() -> sIsTabletForTesting = null);
    }

    static void setToolbarBackgroundColorForTesting(@ColorInt int color) {
        sToolbarBackgroundColorForTesting = color;
        ResettersForTesting.register(() -> sToolbarBackgroundColorForTesting = null);
    }

    static void setUrlBarColorForTesting(@ColorInt int color) {
        sUrlBarColorForTesting = color;
        ResettersForTesting.register(() -> sUrlBarColorForTesting = null);
    }
}
