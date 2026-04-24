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

    @VisibleForTesting static final int SIDE_PANEL_MIN_WIDTH_DP = 360;

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
            @Nullable Rect startingBounds) {
        log(TAG, "populateContent", content, startingBounds);
        ThreadUtils.assertOnUiThread();
        mCurrentContent = content;

        mContainerView.removeAllViews();
        mContainerView.addView(content.mView);

        // TODO(http://crbug.com/487414343): Refine the side panel width.
        @Px int sidePanelWidth = ViewUtils.dpToPx(mParentActivity, SIDE_PANEL_MIN_WIDTH_DP);
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(SIDE_PANEL_DEFAULT_ANCHOR_SIDE, sidePanelWidth));
    }

    @Override
    public void removeContentAndClose(
            Callback<@Nullable Void> onAnimationFinishedCallback, boolean suppressAnimations) {
        log(TAG, "removeContentAndClose", mPanelType, suppressAnimations);
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(SIDE_PANEL_DEFAULT_ANCHOR_SIDE, /* width= */ 0));
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
    public int determineContainerWidth(@Px int availableWidth, @Px int windowWidth) {
        log(TAG, "determineContainerWidth", availableWidth, windowWidth);
        ThreadUtils.assertOnUiThread();

        // TODO(http://crbug.com/487414343): Refine the implementation.
        // Calculate the final container width based on "availableWidth" and "windowWidth".
        return ViewUtils.dpToPx(mParentActivity, SIDE_PANEL_MIN_WIDTH_DP);
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
}
