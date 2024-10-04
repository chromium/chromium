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
import org.chromium.cc.input.BrowserControlsOffsetTagsInfo;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
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
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

/** The business logic for controlling the top toolbar's cc texture. */
public class TopToolbarOverlayMediator {
    // Forced testing params.
    private static Boolean sIsTabletForTesting;
    private static Integer sToolbarBackgroundColorForTesting;
    private static Integer sUrlBarColorForTesting;

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

    TopToolbarOverlayMediator(
            PropertyModel model,
            Context context,
            LayoutStateProvider layoutStateProvider,
            Callback<ClipDrawableProgressBar.DrawingInfo> progressInfoCallback,
            ObservableSupplier<Tab> tabSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            TopUiThemeColorProvider topUiThemeColorProvider,
            int layoutsToShowOn,
            boolean manualVisibilityControl) {
        mContext = context;
        mLayoutStateProvider = layoutStateProvider;
        mProgressInfoCallback = progressInfoCallback;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mModel = model;
        mIsVisibilityManuallyControlled = manualVisibilityControl;
        mIsOnValidLayout = (mLayoutStateProvider.getActiveLayoutType() & layoutsToShowOn) > 0;
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
        Callback<Tab> activityTabCallback =
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
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate,
                            boolean isVisibilityForced) {
                        // The content offset is passed to the toolbar layer so that it can position
                        // itself for using content offset instead of top controls offset is that
                        // top controls can have a different height than that of the toolbar, (e.g.
                        // when status indicator is visible or tab strip is hidden), and the toolbar
                        // needs to be positioned at the bottom of the top controls regardless of
                        // the total height.
                        if (!ToolbarFeatures.isBrowserControlsInVizEnabled(isTablet())
                                || needsAnimate
                                || isVisibilityForced) {
                            mModel.set(
                                    TopToolbarOverlayProperties.CONTENT_OFFSET,
                                    mBrowserControlsStateProvider.getContentOffset());
                        }
                        if (!ChromeFeatureList.sBcivZeroBrowserFrames.isEnabled()) {
                            updateShadowState();
                            updateVisibility();
                        }
                    }

                    @Override
                    public void onAndroidControlsVisibilityChanged(int visibility) {
                        if (ToolbarFeatures.shouldSuppressCaptures()) {
                            mIsBrowserControlsAndroidViewVisible = visibility == View.VISIBLE;
                            if (!ChromeFeatureList.sBcivZeroBrowserFrames.isEnabled()) {
                                updateShadowState();
                            }
                        }
                    }

                    @Override
                    public void onControlsConstraintsChanged(
                            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                            BrowserControlsOffsetTagsInfo offsetTagsInfo,
                            @BrowserControlsState int constraints) {
                        if (ToolbarFeatures.isBrowserControlsInVizEnabled(isTablet())) {
                            mModel.set(
                                    TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG,
                                    offsetTagsInfo.getTopControlsOffsetTag());

                            // With BCIV enabled, scrolling will not update the content offset of
                            // the browser's compositor frame. If we transition to a HIDDEN state
                            // while the controls are already scrolled offscreen, then there is no
                            // need to move the top controls, which means the renderer will not
                            // notify the browser to move them. We set the content offset here so
                            // the browser will submit a compositor frame with the correct offset.
                            int contentOffset = mBrowserControlsStateProvider.getContentOffset();
                            if (constraints == BrowserControlsState.HIDDEN
                                    && contentOffset
                                            == mBrowserControlsStateProvider
                                                    .getTopControlsMinHeight()) {
                                mModel.set(
                                        TopToolbarOverlayProperties.CONTENT_OFFSET, contentOffset);
                            }
                        }
                    }
                };
        mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
        if (ToolbarFeatures.shouldSuppressCaptures()) {
            mIsBrowserControlsAndroidViewVisible =
                    mBrowserControlsStateProvider.getAndroidControlsVisibility() == View.VISIBLE;
        }
    }

    /**
     * Set whether the android view corresponding with this overlay is showing.
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
        boolean drawControlsAsTexture;
        if (ToolbarFeatures.shouldSuppressCaptures()) {
            drawControlsAsTexture = !mIsBrowserControlsAndroidViewVisible;
        } else {
            drawControlsAsTexture =
                    BrowserControlsUtils.drawControlsAsTexture(mBrowserControlsStateProvider);
        }
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
        if (mIsVisibilityManuallyControlled) {
            mModel.set(TopToolbarOverlayProperties.VISIBLE, mManualVisibility && mIsOnValidLayout);
        } else {
            boolean visibility =
                    !BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsStateProvider)
                            && mIsOnValidLayout;
            mModel.set(TopToolbarOverlayProperties.VISIBLE, visibility);
        }
    }

    private void updateAnonymize(Tab tab) {
        if (!mIsVisibilityManuallyControlled && ToolbarFeatures.shouldSuppressCaptures()) {
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
