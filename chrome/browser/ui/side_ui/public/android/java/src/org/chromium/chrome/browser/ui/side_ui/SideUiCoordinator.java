// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.util.ArrayMap;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.ElementType;
import java.lang.annotation.Target;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * Coordinator for "side UI," with "side UI" referring to views that will anchor to either the left
 * or right side of the main browser window.
 */
@NullMarked
public interface SideUiCoordinator extends SideUiStateProvider {

    /**
     * Minimum width (in dp) reserved for {@code WebContents} when calculating {@link SideUiSpecs}
     * and determining {@link SideUiContainer}s' visibility.
     */
    int MIN_WEB_CONTENTS_WIDTH_DP = 412;

    /**
     * The IDs assigned to known {@link SideUiContainer}s listed in descending order of their
     * priorities by which they consume available space. The smaller number indicates higher
     * priority.
     */
    @IntDef({
        SideUiId.VERTICAL_TABS,
        SideUiId.SIDE_PANEL,
        SideUiId.SIDE_UI_FOR_TESTING_HIGH_PRIORITY,
        SideUiId.SIDE_UI_FOR_TESTING_LOW_PRIORITY
    })
    @Target(ElementType.TYPE_USE)
    @interface SideUiId {
        int VERTICAL_TABS = 0;
        int SIDE_PANEL = 1;
        int SIDE_UI_FOR_TESTING_HIGH_PRIORITY = 2;
        int SIDE_UI_FOR_TESTING_LOW_PRIORITY = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * The sides of the window that a {@link SideUiContainer} will anchor to. Each value should have
     * a corresponding container view in main_forked_with_secondary_ui_container.xml.
     */
    @IntDef({AnchorSide.LEFT, AnchorSide.RIGHT})
    @Target(ElementType.TYPE_USE)
    @interface AnchorSide {
        int LEFT = 0;
        int RIGHT = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * POD-type that holds the showability for {@link SideUiContainer}s.
     *
     * <p>What "showability" means:
     *
     * <ul>
     *   <li>Showable: There is enough space to show a {@link SideUiContainer}, but it may not be
     *       actually shown.
     *   <li>Unshowable: There is not enough space to show a {@link SideUiContainer}, and that
     *       container is guaranteed to be hidden.
     * </ul>
     *
     * <p>One use case of showability is using it to control the entry point visibility of a feature
     * that needs a {@link SideUiContainer}.
     */
    final class SideUiShowability {
        /** IDs of showable {@link SideUiContainer}s. */
        public final List<@SideUiId Integer> mShowableSideUiIds;

        /** IDs of unshowable {@link SideUiContainer}s. */
        public final List<@SideUiId Integer> mUnshowableSideUiIds;

        public SideUiShowability(
                List<@SideUiId Integer> showableSideUiIds,
                List<@SideUiId Integer> unshowableSideUiIds) {
            mShowableSideUiIds = List.copyOf(showableSideUiIds);
            mUnshowableSideUiIds = List.copyOf(unshowableSideUiIds);
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (this == obj) {
                return true;
            }

            if (!(obj instanceof SideUiShowability other)) {
                return false;
            }

            return mShowableSideUiIds.equals(other.mShowableSideUiIds)
                    && mUnshowableSideUiIds.equals(other.mUnshowableSideUiIds);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mShowableSideUiIds, mUnshowableSideUiIds);
        }
    }

    /** POD-type that holds the request for {@link #updateUi}. */
    final class UiUpdateRequest {
        /**
         * ID of the {@link SideUiContainer} that requested the UI update.
         *
         * <p>This should be null if the request isn't from a {@link SideUiContainer}.
         */
        final @Nullable @SideUiId Integer mSideUiId;

        /** Whether animations should be suppressed during the UI update. */
        final boolean mSuppressAnimations;

        public UiUpdateRequest(@Nullable @SideUiId Integer sideUiId, boolean suppressAnimations) {
            mSideUiId = sideUiId;
            mSuppressAnimations = suppressAnimations;
        }
    }

    /**
     * POD-type that holds the info about the Side UI specs to be used by a {@link SideUiObserver}.
     * Specifically, this holds the widths (in px) for the parent ViewGroups (one for left-anchored
     * UI and one for right-anchored UI for now ) that hold a {@link SideUiContainer}'s View, based
     * on the SideUiContainer's specified {@link AnchorSide}.
     *
     * <p><strong>Note:</strong> This is a passive data spec and does not guarantee that these specs
     * are currently applied to the active UI. To query the actual active UI state, use {@link
     * SideUiStateProvider} instead.
     */
    final class SideUiSpecs {
        /** Maps @AnchorSide to ContainerWidth. */
        private final Map<@AnchorSide Integer, Integer> mSideUiWidths = new ArrayMap<>();

        public SideUiSpecs(Map<@AnchorSide Integer, Integer> sideUiWidths) {
            mSideUiWidths.putAll(sideUiWidths);
        }

        public SideUiSpecs(@Px int leftContainerWidth, @Px int rightContainerWidth) {
            assert leftContainerWidth >= 0;
            assert rightContainerWidth >= 0;
            mSideUiWidths.put(AnchorSide.LEFT, leftContainerWidth);
            mSideUiWidths.put(AnchorSide.RIGHT, rightContainerWidth);
        }

        public int getWidth(@AnchorSide int side) {
            return mSideUiWidths.getOrDefault(side, 0);
        }

        /**
         * Returns all the entries in the SideUiSpecs. Each entry has a mapping from
         * {@link @AnchorSide} to width.
         */
        public Set<Map.Entry<@AnchorSide Integer, Integer>> entrySet() {
            return mSideUiWidths.entrySet();
        }

        /**
         * Calculates the difference between this {@link SideUiSpecs} and the given {@link
         * SideUiSpecs}.
         *
         * <p>For each {@link AnchorSide}, if the widths are different, the returned {@link
         * SideUiSpecs} retains the width of this {@link SideUiSpecs}. Otherwise, the width is set
         * to 0.
         *
         * <p>The returned {@link SideUiSpecs} is useful for only updating the parts in the UI that
         * are changed.
         *
         * @param sideUiSpecs The {@link SideUiSpecs} to compare against.
         * @return A {@link SideUiSpecs} representing the diff.
         */
        public SideUiSpecs diffAgainst(SideUiSpecs sideUiSpecs) {
            Map<@AnchorSide Integer, Integer> diffWidths = new ArrayMap<>();

            for (@AnchorSide int side = 0; side < AnchorSide.NUM_ENTRIES; side++) {
                Integer thisWidth = mSideUiWidths.get(side);
                Integer otherWidth = sideUiSpecs.mSideUiWidths.get(side);

                if (thisWidth == null && otherWidth == null) {
                    continue;
                }

                if (thisWidth == null) {
                    diffWidths.put(side, 0);
                } else if (!thisWidth.equals(otherWidth)) {
                    diffWidths.put(side, thisWidth);
                }
            }

            return new SideUiSpecs(diffWidths);
        }

        /** Returns true if the width for any {@link AnchorSide} doesn't exist. */
        public boolean isEmpty() {
            return mSideUiWidths.isEmpty();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof SideUiSpecs that)) return false;
            return this.mSideUiWidths.equals(that.mSideUiWidths);
        }

        @Override
        public String toString() {
            return String.format(
                    Locale.ENGLISH,
                    "[LeftContainerWidth: %d, RightContainerWidth: %d]",
                    mSideUiWidths.get(AnchorSide.LEFT),
                    mSideUiWidths.get(AnchorSide.RIGHT));
        }
    }

    /**
     * Registers a {@link SideUiContainer} to be maintained by this coordinator.
     *
     * @param sideUiContainer The {@link SideUiContainer} to register.
     * @throw IllegalArgumentException if the given sideUiContainer has conflicts with the existing
     *     ones, such as duplicated {@link SideUiId} or {@link AnchorSide}.
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
     * Updates all {@link SideUiContainer}s and {@link SideUiObserver}s.
     *
     * <p>Each {@link SideUiContainer} or {@link SideUiObserver} will also be notified of relevant
     * events before/during/after the new {@link SideUiSpecs} is applied to the UI. Please see their
     * documentation for details.
     *
     * @param request The {@link UiUpdateRequest} for the update.
     */
    void updateUi(UiUpdateRequest request);

    /** Immediately ends all ongoing animations. */
    void endAnimations();

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
