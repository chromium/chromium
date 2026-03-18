// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.view.View;
import android.view.ViewGroup.LayoutParams;

/** Minimum implementation of {@link SideUiContainer} to allow setting/getting width for tests. */
public class TestSideUiContainer implements SideUiContainer {
    private final View mSideUiContainerView;

    public TestSideUiContainer(View view) {
        mSideUiContainerView = view;
    }

    @Override
    public View getView() {
        return mSideUiContainerView;
    }

    @Override
    public int determineContainerWidth(int availableWidth, int windowWidth) {
        return 0;
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
