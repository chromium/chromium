// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/** An interface for notifications about the state of the bottom sheet. */
@NullMarked
public interface BottomSheetObserver {
    /**
     * A notification that the sheet has been opened, meaning the sheet is any height greater than
     * its peeking state.
     *
     * @param reason The {@link StateChangeReason} that the sheet was opened.
     */
    default void onSheetOpened(@StateChangeReason int reason) {}

    /**
     * A notification that the sheet has closed, meaning the sheet has reached its peeking state.
     *
     * @param reason The {@link StateChangeReason} that the sheet was closed.
     */
    default void onSheetClosed(@StateChangeReason int reason) {}

    /**
     * An event for when the sheet's offset from the bottom of the screen changes.
     *
     * @param heightFraction The fraction of the way to the fully expanded state that the sheet is.
     *     This will be 0.0f when the sheet is hidden or scrolled off-screen and 1.0f when the sheet
     *     is completely expanded.
     * @param offsetPx The offset of the top of the sheet from the bottom of the screen in pixels.
     */
    default void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    /**
     * An event for when the sheet changes state.
     *
     * <p>When the sheet becomes {@link SheetState#HIDDEN} because its content was hidden, every
     * observer is notified before the sheet shows its next content. During this call, {@link
     * BottomSheetController#getCurrentSheetContent()} still returns the content that was hidden. It
     * isn't destroyed until every observer has been notified. {@link #onSheetContentChanged} for
     * the next content (or null) follows. Compare {@link
     * BottomSheetController#getCurrentSheetContent()} with your content to tell whether this {@link
     * SheetState#HIDDEN} concerns it. Your content can also be hidden without being dismissed, e.g.
     * when the sheet is suppressed or your content is replaced by higher priority content and
     * queued again.
     *
     * @param newState The new sheet state. See {@link SheetState}.
     * @param reason The {@link StateChangeReason} that the sheet's state changed.
     */
    default void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {}

    /**
     * Called after every observer has been notified of a state change through {@link
     * #onSheetStateChanged}. State changes that observers make while being notified are committed
     * together once the outermost one has been dispatched.
     *
     * @param newState The sheet's state once every observer has been notified. See {@link
     *     SheetState}.
     * @param reason The {@link StateChangeReason} of the latest state change.
     */
    default void onSheetStateChangeCommitted(
            @SheetState int newState, @StateChangeReason int reason) {}

    /**
     * An event for when the sheet content changes.
     *
     * @param newContent The new {@link BottomSheetContent}, or null if the sheet has no content.
     */
    default void onSheetContentChanged(@Nullable BottomSheetContent newContent) {}

    /**
     * Called when the sheet layout changes.
     *
     * @param newWidth The new width of the sheet container in pixels.
     * @param newHeight The new height of the sheet container in pixels.
     */
    default void onContainerSizeChanged(int newWidth, int newHeight) {}

    /**
     * Called when the bottom margin of the sheet container changes. This is the space at the bottom
     * of the sheet covered by UI like the keyboard.
     *
     * @param bottomMargin The new bottom margin in pixels.
     */
    default void onContainerBottomMarginChanged(@Px int bottomMargin) {}

    /** Called when the sheet background color override is changed. */
    default void onSheetBackgroundColorOverrideChanged() {}

    /**
     * Called before the inset animation starts. This event is triggered before any layout changes
     * occur.
     */
    default void beforeInsetAnimationStart() {}

    /** Called when the inset animation ends. */
    default void onInsetAnimationEnd() {}
}

