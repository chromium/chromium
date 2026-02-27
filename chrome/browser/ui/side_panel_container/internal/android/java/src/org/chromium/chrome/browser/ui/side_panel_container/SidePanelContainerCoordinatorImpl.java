// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiContainerProperties;
import org.chromium.ui.base.ViewUtils;

/** Implementation of {@link SidePanelContainerCoordinator}. */
@NullMarked
final class SidePanelContainerCoordinatorImpl
        implements SidePanelContainerCoordinator, SideUiContainer {

    @VisibleForTesting static final int SIDE_PANEL_MIN_WIDTH_DP = 360;

    private static final @AnchorSide int SIDE_PANEL_DEFAULT_ANCHOR_SIDE = AnchorSide.END;

    private final Activity mParentActivity;
    private final FrameLayout mContainerView;
    private final SideUiCoordinator mSideUiCoordinator;

    SidePanelContainerCoordinatorImpl(
            Activity parentActivity, SideUiCoordinator sideUiCoordinator) {
        mParentActivity = parentActivity;
        mSideUiCoordinator = sideUiCoordinator;
        mContainerView =
                (FrameLayout)
                        LayoutInflater.from(mParentActivity)
                                .inflate(R.layout.side_panel_container, /* root= */ null);
    }

    @Override
    public void init() {
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.registerSideUiContainer(this);
    }

    @Override
    public void populateContent(SidePanelContent content) {
        ThreadUtils.assertOnUiThread();

        mContainerView.removeAllViews();
        mContainerView.addView(content.mView);

        // TODO(http://crbug.com/487414343): Refine the side panel width.
        @Px int sidePanelWidth = ViewUtils.dpToPx(mParentActivity, SIDE_PANEL_MIN_WIDTH_DP);
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(SIDE_PANEL_DEFAULT_ANCHOR_SIDE, sidePanelWidth));
    }

    @Override
    public void removeContent() {
        ThreadUtils.assertOnUiThread();
        mContainerView.removeAllViews();
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(SIDE_PANEL_DEFAULT_ANCHOR_SIDE, /* width= */ 0));
    }

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.unregisterSideUiContainer(this);
    }

    @Override
    public View getView() {
        ThreadUtils.assertOnUiThread();
        return mContainerView;
    }

    @Override
    @Px
    public int determineContainerWidth(@Px int availableWidth, @Px int windowWidth) {
        ThreadUtils.assertOnUiThread();

        // TODO(http://crbug.com/487414343): Refine the implementation.
        // Calculate the final container width based on "availableWidth" and "windowWidth".
        return ViewUtils.dpToPx(mParentActivity, SIDE_PANEL_MIN_WIDTH_DP);
    }

    @Override
    @Px
    public int getCurrentWidth() {
        ThreadUtils.assertOnUiThread();
        return mContainerView.getWidth();
    }

    @Override
    public void setWidth(@Px int width) {
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

        // TODO(http://crbug.com/488047364): Notify the SidePanelContent View of the width change.
    }
}
