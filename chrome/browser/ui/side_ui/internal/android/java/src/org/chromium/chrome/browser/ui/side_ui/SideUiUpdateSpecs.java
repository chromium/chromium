// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Contains the start and end specs of a UI update. */
@DoNotMock
@NullMarked
final class SideUiUpdateSpecs {

    /** The current {@link SideUiSpecs}. */
    final SideUiSpecs mCurrentSpecs;

    /** The new, complete {@link SideUiSpecs}. */
    final SideUiSpecs mNewSpecs;

    /** {@link SideUiSpecs} containing only the {@link AnchorSide}s that need to be updated. */
    final SideUiSpecs mSpecsDiff;

    /**
     * {@link AnchorContainerTopMargins} containing only the {@link AnchorSide}s that needs to be
     * updated.
     */
    final AnchorContainerTopMargins mTopMarginDiff;

    SideUiUpdateSpecs(
            SideUiSpecs currentSpecs,
            SideUiSpecs newSpecs,
            SideUiSpecs specsDiff,
            AnchorContainerTopMargins topMarginDiff) {
        assert specsDiff.equals(newSpecs.diffAgainst(currentSpecs));
        mCurrentSpecs = currentSpecs;
        mNewSpecs = newSpecs;
        mSpecsDiff = specsDiff;
        mTopMarginDiff = topMarginDiff;
    }
}
