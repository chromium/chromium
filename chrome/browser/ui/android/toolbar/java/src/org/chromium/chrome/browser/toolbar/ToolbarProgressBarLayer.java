// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo;

import java.util.function.Supplier;

/**
 * Browser control presentation of the toolbar progress bar. This class manages the progress bar
 * drawing position based on its relative position in the top controls and the toolbar.
 */
@NullMarked
public class ToolbarProgressBarLayer implements TopControlLayer {

    private final ToolbarControlContainer mControlContainer;
    private final View mProgressBarContainer;
    private final ToolbarProgressBar mProgressBarView;
    private final View mToolbarHairline;
    private final Supplier<@ControlsPosition Integer> mControlsPositionSupplier;
    private final Supplier<Integer> mBookmarkBarIdSupplier;
    private final TopControlsStacker mTopControlsStacker;
    private final BottomControlsStacker mBottomControlsStacker;
    private final boolean mIsToolbarPositionCustomizationEnabled;
    private final Handler mHandler = new Handler();

    /**
     * Construct the browser control layer that represents the toolbar progress bar.
     *
     * @param controlContainer The control container instance.
     * @param progressBarContainer The parent view of ToolbarProgressBar
     * @param toolbarProgressBar The ToolbarProgressBar view instance.
     * @param hairlineView The view for toolbar hairline.
     * @param controlsPositionSupplier The supplier for the current ControlsPosition.
     * @param bookmarkBarIdSupplier The supplier for the bookmark bar Id.
     * @param topControlStacker The TopControlStacker instance.
     * @param bottomControlsStacker The BottomControlsStacker instance.
     * @param isToolbarPositionCustomizationEnabled Whether the toolbar position is customizable.
     */
    public ToolbarProgressBarLayer(
            ToolbarControlContainer controlContainer,
            View progressBarContainer,
            ToolbarProgressBar toolbarProgressBar,
            View hairlineView,
            Supplier<@ControlsPosition Integer> controlsPositionSupplier,
            Supplier<Integer> bookmarkBarIdSupplier,
            TopControlsStacker topControlStacker,
            BottomControlsStacker bottomControlsStacker,
            boolean isToolbarPositionCustomizationEnabled) {
        mControlContainer = controlContainer;
        mProgressBarContainer = progressBarContainer;
        mProgressBarView = toolbarProgressBar;
        mToolbarHairline = hairlineView;
        mControlsPositionSupplier = controlsPositionSupplier;
        mBookmarkBarIdSupplier = bookmarkBarIdSupplier;
        mTopControlsStacker = topControlStacker;
        mBottomControlsStacker = bottomControlsStacker;
        mIsToolbarPositionCustomizationEnabled = isToolbarPositionCustomizationEnabled;

        mTopControlsStacker.addControl(this);
        updateTopAnchorView();
    }

    public void destroy() {
        mTopControlsStacker.removeControl(this);
    }

    // TopControlLayer implementation:

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.PROGRESS_BAR;
    }

    @Override
    public int getTopControlHeight() {
        // The height likely isn't relevant to the TopControlsStacker since the progress bar does
        // not contribute to the total height of the top controls, but we add it for consistency.
        return mProgressBarView.getDefaultHeight();
    }

    @Override
    public int getTopControlVisibility() {
        // TODO(crbug.com/417238089): Possibly add way to notify stacker of visibility changes.
        return mProgressBarView.isStarted()
                        && mControlsPositionSupplier.get() == ControlsPosition.TOP
                ? TopControlVisibility.VISIBLE
                : TopControlVisibility.HIDDEN;
    }

    @Override
    public boolean contributesToTotalHeight() {
        // The progress bar draws over other views, so it does not add height to the top controls.
        return false;
    }

    @Override
    public void onTopControlLayerHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateTopAnchorView();
    }

    /**
     * Progress the progress bar info based on the current browser control positioning.
     *
     * @param drawingInfo The DrawingInfo to be modified.
     */
    public void onProgressBarInfoUpdate(DrawingInfo drawingInfo) {
        mControlContainer.getProgressBarDrawingInfo(drawingInfo);

        // Control container / progress bar container can have a non-zero translation
        // when sitting at the bottom / during animation.
        // TODO(crbug.com/466162772): This calculation is based on the fact that the progress bar
        // lives with toolbar in the scene layer. As a result, the yOffset for the progress bar
        // needs to be based on the control container's position.
        final float controlContainerY = mControlContainer.getView().getY();
        final float progressBarContainerY = mProgressBarContainer.getY();
        int yOffset =
                (int)
                        MathUtils.clamp(
                                progressBarContainerY - controlContainerY,
                                0,
                                mControlContainer.getView().getHeight());

        if (ChromeFeatureList.sAndroidAnimatedProgressBarInBrowser.isEnabled()) {
            // TODO(peilinwang) update these calculations and move them to the stackers
            // when the progress bar gets decoupled from the toolbar and when top
            // stacker is complete. Note: the hairline is a TopControlsLayer, but is not
            // integrated with the TopControlsStacker yet, which is why we have to
            // explicitly account for its height after calling getHeightFromLayerToX.
            int toolbarPosition = mControlsPositionSupplier.get();
            int hairlineHeight = mToolbarHairline.getHeight();
            // TODO(https://crbug.com/471284846): Default position should be TOP instead of NONE on
            // large form factors.
            if (toolbarPosition == ControlsPosition.TOP
                    || toolbarPosition == ControlsPosition.NONE) {
                yOffset =
                        mTopControlsStacker.getHeightFromLayerToTop(TopControlType.PROGRESS_BAR)
                                - mTopControlsStacker.getHeightFromLayerToTop(
                                        TopControlType.TOOLBAR)
                                + hairlineHeight;

                int captureHeightDiff =
                        mControlContainer.getToolbarCaptureHeight()
                                - mControlContainer.getToolbarHeight()
                                - mControlContainer.getToolbarHairlineHeight();
                yOffset += captureHeightDiff;
            } else if (toolbarPosition == ControlsPosition.BOTTOM) {
                yOffset =
                        -(mBottomControlsStacker.getHeightFromLayerToBottom(LayerType.PROGRESS_BAR)
                                        - mBottomControlsStacker.getHeightFromLayerToBottom(
                                                LayerType.BOTTOM_TOOLBAR))
                                - mProgressBarContainer.getHeight()
                                - hairlineHeight;
            }
        }
        drawingInfo.progressBarRect.offset(0, yOffset);
        drawingInfo.progressBarBackgroundRect.offset(0, yOffset);
    }

    // Progress bar should anchor at the bottom of the top controls.
    private void updateTopAnchorView() {
        // When mIsToolbarPositionCustomizationEnabled, this is handled in
        // ToolbarPositionController. Avoid doing duplicate work.
        if (mIsToolbarPositionCustomizationEnabled) return;

        Runnable progressBarChangeRunnable =
                () -> {
                    if (mControlsPositionSupplier.get() != ControlsPosition.TOP) return;
                    CoordinatorLayout.LayoutParams lp =
                            (CoordinatorLayout.LayoutParams)
                                    mProgressBarContainer.getLayoutParams();
                    if (mTopControlsStacker.isLayerAtBottom(TopControlType.BOOKMARK_BAR)
                            && mBookmarkBarIdSupplier.get() != 0) {
                        int bookmarkBarId = mBookmarkBarIdSupplier.get();
                        lp.setAnchorId(bookmarkBarId);
                    } else {
                        lp.setAnchorId(mControlContainer.getView().getId());
                    }
                    mProgressBarContainer.setLayoutParams(lp);
                };

        // Anchor ID cannot be changed during a layout pass. Post the runnable instead.
        if (((ViewGroup) mProgressBarContainer.getParent()).isInLayout()) {
            mHandler.post(progressBarChangeRunnable);
        } else {
            progressBarChangeRunnable.run();
        }
    }
}
