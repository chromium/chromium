// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Locale;

/**
 * Coordinator for "side UI," with "side UI" referring to views that will anchor to either the left
 * or right side of the main browser window.
 */
@NullMarked
public interface SideUiCoordinator extends SideUiStateProvider {

    /** Minimum width for {@code WebContents}, regardless of side panel visibility. */
    int MIN_WEB_CONTENTS_WIDTH_DP = 412;

    /**
     * The side of the window ({@link #START} or {@link #END}) that a {@link SideUiContainer} will
     * anchor to.
     */
    @IntDef({AnchorSide.START, AnchorSide.END})
    @interface AnchorSide {
        int START = 0;
        int END = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * POD-type that holds the info for a request to reposition or resize a {@link SideUiContainer}.
     */
    final class SideUiContainerProperties {
        final @AnchorSide int mAnchorSide;
        final @Px int mWidth;

        public SideUiContainerProperties(@AnchorSide int anchorSide, @Px int width) {
            mAnchorSide = anchorSide;
            mWidth = width;
        }
    }

    /**
     * POD-type that holds the info about the Side UI specs to be used by a {@link SideUiObserver}.
     * Specifically, this holds the widths (in px) for the two parent ViewGroups (one for
     * start-anchored UI and one for end-anchored UI) that hold a {@link SideUiContainer}'s View,
     * based on the SideUiContainer's specified {@link AnchorSide}. TODO(crbug.com/489808658): Move
     * to SideUiStateProvider.
     */
    final class SideUiSpecs {
        /** A {@link SideUiSpecs} with a startContainerWidth and endContainerWidth of 0. */
        public static final SideUiSpecs EMPTY_SIDE_UI_SPECS =
                new SideUiSpecs(/* startContainerWidth= */ 0, /* endContainerWidth= */ 0);

        public final @Px int mStartContainerWidth;
        public final @Px int mEndContainerWidth;

        public SideUiSpecs(@Px int startContainerWidth, @Px int endContainerWidth) {
            mStartContainerWidth = startContainerWidth;
            mEndContainerWidth = endContainerWidth;
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof SideUiSpecs that)) return false;
            return (this.mStartContainerWidth == that.mStartContainerWidth)
                    && (this.mEndContainerWidth == that.mEndContainerWidth);
        }

        @Override
        public String toString() {
            return String.format(
                    Locale.ENGLISH,
                    "[StartContainerWidth: %d, EndContainerWidth: %d]",
                    mStartContainerWidth,
                    mEndContainerWidth);
        }
    }

    /**
     * Registers a {@link SideUiContainer} to be maintained by this coordinator.
     *
     * @param sideUiContainer The {@link SideUiContainer} to register.
     */
    void registerSideUiContainer(SideUiContainer sideUiContainer);

    /**
     * Unregisters a {@link SideUiContainer} such that it will no longer be maintained by this
     * coordinator.
     *
     * @param sideUiContainer The {@link SideUiContainer} to unregister.
     */
    void unregisterSideUiContainer(SideUiContainer sideUiContainer);

    /**
     * Requests that the registered {@link SideUiContainer} change its width.
     * <strong>Important:</strong> this should only be called by the feature that owns the affected
     * {@link SideUiContainer}.
     *
     * @param properties The {@link SideUiContainerProperties} that defines the new requested
     *     position for the registered {@link SideUiContainer}.
     * @param suppressAnimations Whether animations should be suppressed for the container update.
     *     If true, the update will happen immediately, without animations.
     */
    void requestUpdateContainer(SideUiContainerProperties properties, boolean suppressAnimations);

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
