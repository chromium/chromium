// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/** An interface for notifications about the state of the bottom sheet. */
public interface BottomSheetObserver {
    /**
     * A notification that the sheet has been opened, meaning the sheet is any height greater
     * than its peeking state.
     * @param reason The {@link StateChangeReason} that the sheet was opened.
     */
    void onSheetOpened(@StateChangeReason int reason);

    /**
     * A notification that the sheet has closed, meaning the sheet has reached its peeking state.
     * @param reason The {@link StateChangeReason} that the sheet was closed.
     */
    void onSheetClosed(@StateChangeReason int reason);

    /**
     * An event for when the sheet's offset from the bottom of the screen changes.
     *
     * @param heightFraction The fraction of the way to the fully expanded state that the sheet
     *                       is. This will be 0.0f when the sheet is hidden or scrolled off-screen
     *                       and 1.0f when the sheet is completely expanded.
     * @param offsetPx The offset of the top of the sheet from the bottom of the screen in pixels.
     */
    void onSheetOffsetChanged(float heightFraction, float offsetPx);

    /**
     * An event for when the sheet changes state.
     * @param newState The new sheet state. See {@link SheetState}.
     * @param reason The {@link StateChangeReason} that the sheet's state changed.
     */
    void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason);

    /**
     * An event for when the sheet content changes.
     * @param newContent The new {@link BottomSheetContent}, or null if the sheet has no content.
     */
    void onSheetContentChanged(@Nullable BottomSheetContent newContent);
}
