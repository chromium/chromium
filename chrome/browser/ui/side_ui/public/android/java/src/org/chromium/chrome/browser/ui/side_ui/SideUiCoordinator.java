// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.util.ArrayMap;

import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

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
     * The height type for a {@link SideUiContainer}. {@code NOT_APPLICABLE} is used for
     * invisible/detached SideUiContainer.
     */
    @IntDef({HeightType.NOT_APPLICABLE, HeightType.TOOLBAR, HeightType.WEB_CONTENTS})
    @Target(ElementType.TYPE_USE)
    @interface HeightType {
        /** For when a {@link SideUiContainer} shouldn't be shown. */
        int NOT_APPLICABLE = 0;

        int TOOLBAR = 1;
        int WEB_CONTENTS = 2;
        int NUM_ENTRIES = 3;
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
     * POD-type that holds the info about the Side UI specs.
     *
     * <p><strong>Note:</strong> This is a passive data spec and does not guarantee that these specs
     * are currently applied to the active UI. To query the actual active UI state, use {@link
     * SideUiStateProvider} instead.
     */
    final class SideUiSpecs {
        public static final class SideUiSize {
            public final @Px int mWidth;
            public final @HeightType int mHeightType;

            public SideUiSize(@Px int width, @HeightType int heightType) {
                assert width > 0 || (width == 0 && heightType == HeightType.NOT_APPLICABLE)
                        : "inconsistent width and heightType";

                mWidth = width;
                mHeightType = heightType;
            }

            @Override
            public boolean equals(@Nullable Object obj) {
                if (!(obj instanceof SideUiSize that)) return false;
                return this.mWidth == that.mWidth && this.mHeightType == that.mHeightType;
            }

            @Override
            public int hashCode() {
                return Objects.hash(mWidth, mHeightType);
            }

            @Override
            public String toString() {
                return String.format(
                        Locale.ENGLISH, "[width: %d, heightType: %d]", mWidth, mHeightType);
            }
        }

        /** Maps @AnchorSide to SideUiSize. */
        private final Map<@AnchorSide Integer, SideUiSize> mSideUiSpecs = new ArrayMap<>();

        public SideUiSpecs(Map<@AnchorSide Integer, SideUiSize> sideUiSpecs) {
            mSideUiSpecs.putAll(sideUiSpecs);
        }

        @VisibleForTesting
        @Deprecated
        public SideUiSpecs(@Px int leftContainerWidth, @Px int rightContainerWidth) {
            assert leftContainerWidth >= 0;
            assert rightContainerWidth >= 0;
            var specs = new ArrayMap<@AnchorSide Integer, SideUiSize>();
            specs.put(
                    AnchorSide.LEFT,
                    new SideUiSize(
                            leftContainerWidth,
                            leftContainerWidth == 0
                                    ? HeightType.NOT_APPLICABLE
                                    : HeightType.TOOLBAR));
            specs.put(
                    AnchorSide.RIGHT,
                    new SideUiSize(
                            rightContainerWidth,
                            rightContainerWidth == 0
                                    ? HeightType.NOT_APPLICABLE
                                    : HeightType.TOOLBAR));
            mSideUiSpecs.putAll(specs);
        }

        public int getWidth(@AnchorSide int side) {
            SideUiSize spec = mSideUiSpecs.get(side);
            return spec != null ? spec.mWidth : 0;
        }

        public @HeightType int getHeightType(@AnchorSide int side) {
            SideUiSize spec = mSideUiSpecs.get(side);
            return spec != null ? spec.mHeightType : HeightType.NOT_APPLICABLE;
        }

        /**
         * Returns all the entries in the SideUiSpecs. Each entry has a mapping from
         * {@link @AnchorSide} to {@link SideUiSize}.
         */
        public Set<Map.Entry<@AnchorSide Integer, SideUiSize>> entrySet() {
            return mSideUiSpecs.entrySet();
        }

        /**
         * Calculates the difference between this {@link SideUiSpecs} and the given {@link
         * SideUiSpecs}.
         *
         * <p>For each {@link AnchorSide}, if the specs are different, the returned {@link
         * SideUiSpecs} retains the spec of this {@link SideUiSpecs}. If this spec does not exist,
         * the width is set to 0, and the height type to NOT_APPLICABLE.
         *
         * <p>The returned {@link SideUiSpecs} is useful for only updating the parts in the UI that
         * are changed.
         *
         * @param sideUiSpecs The {@link SideUiSpecs} to compare against.
         * @return A {@link SideUiSpecs} representing the diff.
         */
        public SideUiSpecs diffAgainst(SideUiSpecs sideUiSpecs) {
            Map<@AnchorSide Integer, SideUiSize> diffSpecs = new ArrayMap<>();

            for (@AnchorSide int side = 0; side < AnchorSide.NUM_ENTRIES; side++) {
                SideUiSize thisSpec = mSideUiSpecs.get(side);
                SideUiSize otherSpec = sideUiSpecs.mSideUiSpecs.get(side);

                if (thisSpec == null && otherSpec == null) {
                    continue;
                }

                if (thisSpec == null) {
                    assert otherSpec != null;
                    diffSpecs.put(side, new SideUiSize(0, HeightType.NOT_APPLICABLE));
                } else if (!thisSpec.equals(otherSpec)) {
                    diffSpecs.put(side, thisSpec);
                }
            }

            return new SideUiSpecs(diffSpecs);
        }

        /** Returns true if the spec for any {@link AnchorSide} doesn't exist. */
        public boolean isEmpty() {
            return mSideUiSpecs.isEmpty();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof SideUiSpecs that)) return false;
            return this.mSideUiSpecs.equals(that.mSideUiSpecs);
        }

        @Override
        public String toString() {
            return String.format(
                    Locale.ENGLISH,
                    "[LeftContainerSpec: %s, RightContainerSpec: %s]",
                    mSideUiSpecs.get(AnchorSide.LEFT),
                    mSideUiSpecs.get(AnchorSide.RIGHT));
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
