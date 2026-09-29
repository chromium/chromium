// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator controlling the property model of the download toolbar button. */
@NullMarked
class DownloadButtonMediator {
    private final PropertyModel mModel;
    private boolean mHasSpaceToShow;

    /**
     * Creates a new {@link DownloadButtonMediator}.
     *
     * @param model The {@link PropertyModel} for the download button.
     * @param onClickRunnable Callback to invoke when the button is clicked.
     */
    DownloadButtonMediator(PropertyModel model, Runnable onClickRunnable) {
        mModel = model;
        mModel.set(DownloadButtonProperties.ON_CLICK, v -> onClickRunnable.run());
    }

    void setShouldShow(boolean shouldShow) {
        mModel.set(DownloadButtonProperties.SHOULD_SHOW, shouldShow);
        updateButtonVisibility();
    }

    boolean shouldShow() {
        return mModel.get(DownloadButtonProperties.SHOULD_SHOW);
    }

    boolean isVisible() {
        return mModel.get(DownloadButtonProperties.IS_VISIBLE);
    }

    boolean hasSpaceToShow() {
        return mHasSpaceToShow;
    }

    void setHasSpaceToShow(boolean hasSpaceToShow) {
        mHasSpaceToShow = hasSpaceToShow;
        updateButtonVisibility();
    }

    /** Updates the button visibility on the model based on shouldShow and space availability. */
    private void updateButtonVisibility() {
        boolean isVisible = mModel.get(DownloadButtonProperties.SHOULD_SHOW) && mHasSpaceToShow;
        mModel.set(DownloadButtonProperties.IS_VISIBLE, isVisible);
    }
}
