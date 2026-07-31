// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.Px;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.ui.base.ViewUtils;

/** Minimum implementation of {@link SideUiContainer} to allow setting/getting width for tests. */
@DoNotMock
@NullMarked
public final class TestSideUiContainer implements SideUiContainer {
    private static final int DEFAULT_MAX_WIDTH_DP = 412;

    /** Height type for this container. */
    public @HeightType int mHeightType = HeightType.TOOLBAR;

    /**
     * Whether the container has content to show.
     *
     * <p>This will be returned by {@link #hasContentToShow()}.
     */
    public boolean mHasContentToShow = true;

    /** The last {@code availableWidth} received by {@link #determineShowableSize}. */
    public @Nullable @Px Integer mLastAvailableWidth;

    /** The last {@code windowWidth} received by {@link #determineShowableSize}. */
    public @Nullable @Px Integer mLastWindowWidth;

    /** The last {@code isFullscreen} received by {@link #determineShowableSize}. */
    public boolean mLastIsFullscreen;

    /** Maximum width for this {@link SideUiContainer}. */
    public int mMaxWidthDp = DEFAULT_MAX_WIDTH_DP;

    /** Minimum width for this {@link SideUiContainer}. */
    public int mMinWidthDp;

    /** Number of times {@link #onWillAutoClose} is called. */
    public int mNumOnWillAutoCloseReceived;

    /** Number of times {@link #onWillAutoRestore} is called. */
    public int mNumOnWillAutoRestoreReceived;

    /**
     * Whether to call {@link SideUiCoordinator#updateUi} in {@link #onWillAutoClose()}.
     *
     * <p>When this is true, it simulates a common mistake in a {@link SideUiContainer}. Please see
     * the documentation of {@link #onWillAutoClose()} for details.
     */
    public boolean mRequestUiUpdateOnWillAutoClose;

    /** Number of times {@link #onUiUpdateCompleted} is called. */
    public int mNumOnUiUpdateCompletedReceived;

    /** The last {@code oldWidth} received by {@link #onUiUpdateCompleted}. */
    public @Nullable @Px Integer mLastOldWidth;

    /** The last {@code newWidth} received by {@link #onUiUpdateCompleted}. */
    public @Nullable @Px Integer mLastNewWidth;

    private final SideUiCoordinator mSideUiCoordinator;
    private final View mSideUiContainerView;
    private final @SideUiId int mSideUiId;
    private final @AnchorSide int mAnchorSide;

    public TestSideUiContainer(
            SideUiCoordinator sideUiCoordinator,
            View sideUiContainerView,
            @SideUiId int sideUiId,
            @AnchorSide int anchorSide) {
        mSideUiCoordinator = sideUiCoordinator;
        mSideUiContainerView = sideUiContainerView;
        mSideUiId = sideUiId;
        mAnchorSide = anchorSide;
    }

    @Override
    public View getView() {
        return mSideUiContainerView;
    }

    @Override
    public @SideUiId int getSideUiId() {
        return mSideUiId;
    }

    @Override
    public SideUiSize determineShowableSize(
            @Px int availableWidth, @Px int windowWidth, boolean isFullscreen) {
        assert availableWidth <= windowWidth;
        assert mMinWidthDp <= mMaxWidthDp;
        assert mMaxWidthDp <= windowWidth;

        mLastAvailableWidth = availableWidth;
        mLastWindowWidth = windowWidth;
        mLastIsFullscreen = isFullscreen;

        @Px int minWidth = ViewUtils.dpToPx(mSideUiContainerView.getContext(), mMinWidthDp);
        @Px int maxWidth = ViewUtils.dpToPx(mSideUiContainerView.getContext(), mMaxWidthDp);

        if (availableWidth < minWidth) {
            return new SideUiSize(0, HeightType.NOT_APPLICABLE);
        }

        return new SideUiSize(availableWidth < maxWidth ? availableWidth : maxWidth, mHeightType);
    }

    @Override
    @AnchorSide
    public int getAnchorSide() {
        return mAnchorSide;
    }

    @Override
    public boolean hasContentToShow() {
        return mHasContentToShow;
    }

    @Override
    public void setWidth(int width) {
        LayoutParams layoutParams = mSideUiContainerView.getLayoutParams();
        layoutParams.width = width;
        mSideUiContainerView.setLayoutParams(layoutParams);
    }

    @Override
    public void onUiUpdateCompleted(
            @Px int oldWidth,
            @Px int newWidth,
            @HeightType int oldHeightType,
            @HeightType int newHeightType) {
        mNumOnUiUpdateCompletedReceived++;
        mLastOldWidth = oldWidth;
        mLastNewWidth = newWidth;
    }

    @Override
    public void onWillAutoClose() {
        mNumOnWillAutoCloseReceived++;

        if (mRequestUiUpdateOnWillAutoClose) {
            mSideUiCoordinator.updateUi(
                    new UiUpdateRequest(mSideUiId, /* suppressAnimations= */ true));
        }
    }

    @Override
    public void onWillAutoRestore() {
        mNumOnWillAutoRestoreReceived++;
    }
}
