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

/** Minimum implementation of {@link SideUiContainer} to allow setting/getting width for tests. */
@NullMarked
public final class TestSideUiContainer implements SideUiContainer {
    public static final @Px int TEST_SIDE_UI_WIDTH = 412;

    public static final @AnchorSide int TEST_ANCHOR_SIDE = AnchorSide.END;

    /** The last {@code requestedWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastRequestedWidth;

    /** The last {@code availableWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastAvailableWidth;

    /** The last {@code windowWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastWindowWidth;

    /** Width to be returned by {@link #determineContainerWidth}, if not null. */
    public @Nullable @Px Integer mDeterminedWidth;

    private final SideUiCoordinator mSideUiCoordinator;
    private final View mSideUiContainerView;

    public TestSideUiContainer(SideUiCoordinator sideUiCoordinator, View view) {
        mSideUiCoordinator = sideUiCoordinator;
        mSideUiContainerView = view;
    }

    @Override
    public View getView() {
        return mSideUiContainerView;
    }

    @Override
    public int determineContainerWidth(
            @Px int requestedWidth, @Px int availableWidth, @Px int windowWidth) {
        mLastRequestedWidth = requestedWidth;
        mLastAvailableWidth = availableWidth;
        mLastWindowWidth = windowWidth;

        return mDeterminedWidth != null ? mDeterminedWidth : requestedWidth;
    }

    @Override
    public int getCurrentWidth() {
        return mSideUiContainerView.getWidth();
    }

    @Override
    @AnchorSide
    public int getAnchorSide() {
        return TEST_ANCHOR_SIDE;
    }

    @Override
    public void setWidth(int width) {
        LayoutParams layoutParams = mSideUiContainerView.getLayoutParams();
        layoutParams.width = width;
        mSideUiContainerView.setLayoutParams(layoutParams);
    }

    @Override
    public void onWindowResized(boolean canShowSideUi) {
        @Px int requestedSideUiWidth = canShowSideUi ? TEST_SIDE_UI_WIDTH : 0;
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(TEST_ANCHOR_SIDE, requestedSideUiWidth),
                /* suppressAnimations= */ true);
    }
}
