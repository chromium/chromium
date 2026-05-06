// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import android.app.Activity;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel.SidePanelCoordinatorAndroid;
import org.chromium.chrome.browser.ui.side_panel.SidePanelType;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiContainerProperties;
import org.chromium.ui.base.ViewUtils;

/** Implementation of {@link SidePanelContainerCoordinator}. */
@NullMarked
final class SidePanelContainerCoordinatorImpl
        implements SidePanelContainerCoordinator, SideUiContainer {
    private static final String TAG = "SidePanelContainerCoordinatorImpl";

    /**
     * Threshold of available width in the window, in dp. Once crossed, it will lead to a change in
     * side panel width.
     */
    private static final int AVAILABLE_WINDOW_WIDTH_THRESHOLD_DP = 1200;

    private static final int SIDE_PANEL_MAX_WIDTH_DP = 412;
    private static final int SIDE_PANEL_MIN_WIDTH_DP = 360;

    private static final @AnchorSide int SIDE_PANEL_DEFAULT_ANCHOR_SIDE = AnchorSide.END;

    private final Activity mParentActivity;
    private final FrameLayout mContainerView;
    private final SideUiCoordinator mSideUiCoordinator;
    private final @SidePanelType int mPanelType;

    // TODO(crbug.com/496407828): Use this to notify native side of events like "animation ended".
    @SuppressWarnings("UnusedVariable")
    private @Nullable SidePanelCoordinatorAndroid mSidePanelCoordinatorAndroid;

    private @Nullable SidePanelContent mCurrentContent;

    /**
     * Constructs a concrete implementation of the SidePanelContainerCoordinator interface.
     *
     * @param parentActivity Parent Activity that will own this instance.
     * @param sideUiCoordinator Coordinator for the Side Panel UI anchoring view.
     * @param panelType The type of panel that this coordinator is associated with.
     */
    SidePanelContainerCoordinatorImpl(
            Activity parentActivity,
            SideUiCoordinator sideUiCoordinator,
            @SidePanelType int panelType) {
        log(TAG, "constructor", parentActivity, sideUiCoordinator, panelType);
        mParentActivity = parentActivity;
        mSideUiCoordinator = sideUiCoordinator;
        mPanelType = panelType;
        mContainerView =
                (FrameLayout)
                        LayoutInflater.from(mParentActivity)
                                .inflate(R.layout.side_panel_container, /* root= */ null);
    }

    @Override
    public void init(SidePanelCoordinatorAndroid sidePanelCoordinatorAndroid) {
        log(TAG, "init");
        ThreadUtils.assertOnUiThread();
        mSidePanelCoordinatorAndroid = sidePanelCoordinatorAndroid;
        mSideUiCoordinator.registerSideUiContainer(this);
    }

    @Override
    public void populateContent(
            SidePanelContent content,
            Callback<@Nullable Void> onAnimationFinishedCallback,
            @Nullable Rect startingBounds,
            boolean suppressAnimations) {
        log(TAG, "populateContent", content, startingBounds, suppressAnimations);
        ThreadUtils.assertOnUiThread();
        mCurrentContent = content;

        mContainerView.removeAllViews();
        mContainerView.addView(content.mView);

        // It's fine to always _request_ the max width. The final width will be determined in
        // determineContainerWidth().
        @Px int sidePanelMaxWidth = ViewUtils.dpToPx(mParentActivity, SIDE_PANEL_MAX_WIDTH_DP);
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(SIDE_PANEL_DEFAULT_ANCHOR_SIDE, sidePanelMaxWidth),
                suppressAnimations);
        // TODO(crbug.com/496407828): Move this around so it actually runs after the animation is
        //  finished.
        onAnimationFinishedCallback.onResult(null);
    }

    @Override
    public void removeContentAndClose(
            Callback<@Nullable Void> onAnimationFinishedCallback, boolean suppressAnimations) {
        log(TAG, "removeContentAndClose", mPanelType, suppressAnimations);
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(SIDE_PANEL_DEFAULT_ANCHOR_SIDE, /* width= */ 0),
                suppressAnimations);
        // TODO(crbug.com/496407828): Move this around so it actually runs after the animation is
        //  finished.
        onAnimationFinishedCallback.onResult(null);
    }

    @Override
    public boolean isShowing(SidePanelContent sidePanelContent) {
        log(TAG, "isShowing", sidePanelContent);
        ThreadUtils.assertOnUiThread();
        return sidePanelContent == mCurrentContent;
    }

    @Override
    public @SidePanelType int getPanelType() {
        return mPanelType;
    }

    @Override
    public void destroy() {
        log(TAG, "destroy");
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.unregisterSideUiContainer(this);
    }

    @Override
    public View getView() {
        log(TAG, "getView");
        ThreadUtils.assertOnUiThread();
        return mContainerView;
    }

    @Override
    @Px
    public int determineContainerWidth(
            @Px int requestedWidth, @Px int availableWidth, @Px int windowWidth) {
        log(TAG, "determineContainerWidth", requestedWidth, availableWidth, windowWidth);
        ThreadUtils.assertOnUiThread();

        if (requestedWidth == 0) {
            return 0;
        }

        int availableWidthDp = ViewUtils.pxToDp(mParentActivity, availableWidth);
        int containerWidthDp = determineContainerWidthDp(availableWidthDp);
        return ViewUtils.dpToPx(mParentActivity, containerWidthDp);
    }

    @Override
    @Px
    public int getCurrentWidth() {
        log(TAG, "getCurrentWidth");
        ThreadUtils.assertOnUiThread();
        return mContainerView.getWidth();
    }

    @Override
    public void setWidth(@Px int width) {
        log(TAG, "setWidth", width);
        ThreadUtils.assertOnUiThread();

        LayoutParams layoutParams = mContainerView.getLayoutParams();
        assert layoutParams != null
                : "setWidth() should be called after the container View is attached";
        assert layoutParams.height == LayoutParams.MATCH_PARENT
                : "the container View's height should match its parent";

        if (layoutParams.width != width) {
            layoutParams.width = width;
            mContainerView.setLayoutParams(layoutParams);
        }

        // Remove the content if setting the width the 0 (i.e. hiding the panel).
        if (width == 0) {
            mContainerView.removeAllViews();
            mCurrentContent = null;
        }

        // TODO(http://crbug.com/488047364): Notify the SidePanelContent View of the width change.
    }

    /**
     * Returns the final width (in dp) of the side panel given the available width in the window.
     */
    @VisibleForTesting
    static int determineContainerWidthDp(int availableWidthDp) {
        if (availableWidthDp >= AVAILABLE_WINDOW_WIDTH_THRESHOLD_DP) {
            return SIDE_PANEL_MAX_WIDTH_DP;
        }

        if (availableWidthDp > SIDE_PANEL_MIN_WIDTH_DP) {
            return SIDE_PANEL_MIN_WIDTH_DP;
        }

        // As of May 1, 2026, there were side panel browser tests running on _phone_ bots, where
        // there may not be enough space for SIDE_PANEL_MIN_WIDTH_DP. So we just give side panel
        // half the available width to make the tests happy.
        // TODO(crbug.com/510044610): Stop running side panel browser tests on _phone_ bots, then
        // delete this logic.
        log(TAG, "available width is less than min side panel width");
        return availableWidthDp / 2;
    }
}
