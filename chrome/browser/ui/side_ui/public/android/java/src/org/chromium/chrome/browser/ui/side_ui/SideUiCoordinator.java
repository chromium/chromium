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
public interface SideUiCoordinator {

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
     * based on the SideUiContainer's specified {@link AnchorSide}.
     */
    final class SideUiSpecs {
        /** A {@link SideUiSpecs} with a startX and endX of 0. */
        public static final SideUiSpecs EMPTY_SIDE_UI_SPECS =
                new SideUiSpecs(/* startX= */ 0, /* endX= */ 0);

        public final @Px int mStartContainerWidth;
        public final @Px int mEndContainerWidth;

        public SideUiSpecs(@Px int startX, @Px int endX) {
            mStartContainerWidth = startX;
            mEndContainerWidth = endX;
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
     * Adds a {@link SideUiObserver}. The provided observer will be notified whenever a new {@link
     * SideUiSpecs} is determined as a result of a change in a registered {@link SideUiContainer}.
     *
     * <p>Upon being added, the provided observer will also be notified of the current state of the
     * side UI so that it may immediately position itself accordingly. e.g. This accounts for the
     * case where an observer registers itself while some {@link SideUiContainer} is already showing
     * and there is no incoming update request that would trigger a notification for observers.
     *
     * <p>This is no-op (including being notified of the current {@link SideUiSpecs}) if the
     * provided observer was already registered.
     *
     * @param observer The {@link SideUiObserver} to add.
     */
    void addObserver(SideUiObserver observer);

    /**
     * Removes a {@link SideUiObserver}.
     *
     * <p>Upon removal, the provided observer will be notified as if there were no side UI present.
     * i.e. it will be passed a {@link SideUiSpecs#EMPTY_SIDE_UI_SPECS}, which represents the specs
     * when no side UI is currently showing. The intent of this is to attempt to return a given
     * observer to its state prior to observing Side UI.
     *
     * <p>This is no-op (including not being notified of empty Side UI specs) if the provided
     * observer was not actually present in the list of observers.
     *
     * @param observer The {@link SideUiObserver} to remove.
     */
    void removeObserver(SideUiObserver observer);

    /**
     * Requests that the registered {@link SideUiContainer} change its width.
     * <strong>Important:</strong> this should only be called by the feature that owns the affected
     * {@link SideUiContainer}.
     *
     * @param properties The {@link SideUiContainerProperties} that defines the new requested
     *     position for the registered {@link SideUiContainer}.
     */
    void requestUpdateContainer(SideUiContainerProperties properties);

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
