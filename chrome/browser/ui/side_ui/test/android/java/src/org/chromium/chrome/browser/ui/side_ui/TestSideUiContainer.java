// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Minimum implementation of {@link SideUiContainer} to allow setting/getting width for tests. */
@NullMarked
public final class TestSideUiContainer implements SideUiContainer {

    /** The last {@code requestedWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastRequestedWidth;

    /** The last {@code availableWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastAvailableWidth;

    /** The last {@code windowWidth} received by {@link #determineContainerWidth}. */
    public @Nullable @Px Integer mLastWindowWidth;

    private final View mSideUiContainerView;

    public TestSideUiContainer(View view) {
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

        return requestedWidth;
    }

    @Override
    public int getCurrentWidth() {
        return mSideUiContainerView.getWidth();
    }

    @Override
    public void setWidth(int width) {
        LayoutParams layoutParams = mSideUiContainerView.getLayoutParams();
        layoutParams.width = width;
        mSideUiContainerView.setLayoutParams(layoutParams);
    }
}
