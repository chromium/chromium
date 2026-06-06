// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiContainerProperties;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;

/** Minimum implementation of {@link SideUiContainer} to allow setting/getting width for tests. */
@NullMarked
public final class TestSideUiContainer implements SideUiContainer {
    public static final @Px int TEST_SIDE_UI_WIDTH = 412;

    /** The last {@code requestedWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastRequestedWidth;

    /** The last {@code availableWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastAvailableWidth;

    /** The last {@code windowWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastWindowWidth;

    /** Minimum width for this {@link SideUiContainer}. */
    public int mMinWidthDp;

    private final SideUiCoordinator mSideUiCoordinator;
    private final View mSideUiContainerView;
    private final @AnchorSide int mAnchorSide;

    public TestSideUiContainer(
            SideUiCoordinator sideUiCoordinator,
            View sideUiContainerView,
            @AnchorSide int anchorSide) {
        mSideUiCoordinator = sideUiCoordinator;
        mSideUiContainerView = sideUiContainerView;
        mAnchorSide = anchorSide;
    }

    @Override
    public View getView() {
        return mSideUiContainerView;
    }

    @Override
    public @SideUiId int getSideUiId() {
        return SideUiId.SIDE_UI_FOR_TESTING;
    }

    @Override
    public int determineContainerWidth(
            @Px int requestedWidth, @Px int availableWidth, @Px int windowWidth) {
        assert availableWidth <= windowWidth;

        mLastRequestedWidth = requestedWidth;
        mLastAvailableWidth = availableWidth;
        mLastWindowWidth = windowWidth;

        if (availableWidth < mMinWidthDp) {
            return 0;
        }

        // mMinWidthDp <= availableWidth < requestedWidth
        if (availableWidth < requestedWidth) {
            return availableWidth;
        }

        // requestedWidth <= availableWidth <= windowWidth
        return requestedWidth;
    }

    @Override
    @AnchorSide
    public int getAnchorSide() {
        return mAnchorSide;
    }

    @Override
    public void setWidth(int width) {
        LayoutParams layoutParams = mSideUiContainerView.getLayoutParams();
        layoutParams.width = width;
        mSideUiContainerView.setLayoutParams(layoutParams);
    }

    @Override
    public void onContainerResized(@Px int containerWidth) {}

    @Override
    public void onWindowResized(boolean canShowSideUi) {
        @Px int requestedSideUiWidth = canShowSideUi ? TEST_SIDE_UI_WIDTH : 0;
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        SideUiId.SIDE_UI_FOR_TESTING, mAnchorSide, requestedSideUiWidth),
                /* suppressAnimations= */ true);
    }
}
