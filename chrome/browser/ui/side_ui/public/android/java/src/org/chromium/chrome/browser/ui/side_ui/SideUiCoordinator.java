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
        /** What triggered a {@link SideUiCoordinator#updateUi} request. */
        @IntDef({
            UpdateReason.UNSPECIFIED,
            UpdateReason.SIDE_UI_REQUEST,
            UpdateReason.ANDROID_CONFIGURATION_CHANGED,
            UpdateReason.TOP_CONTROLS_HEIGHT_CHANGED,
            UpdateReason.FULL_SCREEN_MODE_ENTERED,
            UpdateReason.FULL_SCREEN_MODE_EXITED,
            UpdateReason.RESIZE_LIVE,
            UpdateReason.RESIZE_COMMITTED
        })
        @Target(ElementType.TYPE_USE)
        public @interface UpdateReason {
            /** A request from outside the Side UI framework, with no originating container. */
            int UNSPECIFIED = 0;

            /**
             * A {@link SideUiContainer} requested a UI update. A width of zero denotes a hidden
             * container, so showing and hiding a container are also represented by this value.
             *
             * <p>Unlike the values below, this denotes a request originating from a {@link
             * SideUiContainer} rather than a change to the environment the containers are laid out
             * within.
             */
            int SIDE_UI_REQUEST = 1;

            /**
             * The Android {@link android.content.res.Configuration} changed, for example the window
             * size, the orientation or the display density.
             */
            int ANDROID_CONFIGURATION_CHANGED = 2;

            /** The height of the top browser controls changed. */
            int TOP_CONTROLS_HEIGHT_CHANGED = 3;

            /** The activity entered fullscreen mode. */
            int FULL_SCREEN_MODE_ENTERED = 4;

            /** The activity exited fullscreen mode. */
            int FULL_SCREEN_MODE_EXITED = 5;

            /** An intermediate update during a manual resize. */
            int RESIZE_LIVE = 6;

            /** The final update committing a manual resize. */
            int RESIZE_COMMITTED = 7;

            int NUM_ENTRIES = 8;
        }

        /**
         * ID of the {@link SideUiContainer} that requested the UI update.
         *
         * <p>This should be null if the request isn't from a {@link SideUiContainer}.
         */
        final @Nullable @SideUiId Integer mSideUiId;

        /** Whether animations should be suppressed during the UI update. */
        final boolean mSuppressAnimations;

        /** What triggered this request. */
        public final @UpdateReason int mUpdateReason;

        /**
         * Constructs a request with {@link UpdateReason#SIDE_UI_REQUEST}.
         *
         * @param sideUiId ID of the {@link SideUiContainer} requesting the UI update.
         * @param suppressAnimations Whether animations should be suppressed during the UI update.
         */
        public UiUpdateRequest(@SideUiId int sideUiId, boolean suppressAnimations) {
            this(sideUiId, suppressAnimations, UpdateReason.SIDE_UI_REQUEST);
        }

        /**
         * Constructs a request with {@link UpdateReason#SIDE_UI_REQUEST}, or {@link
         * UpdateReason#UNSPECIFIED} if {@code sideUiId} is null.
         *
         * @param sideUiId ID of the {@link SideUiContainer} requesting the UI update, or null if
         *     the request is not from a {@link SideUiContainer}.
         * @param suppressAnimations Whether animations should be suppressed during the UI update.
         * @deprecated Only {@link SideUiContainer}s should construct a {@link UiUpdateRequest}
         *     outside this package; use {@link #UiUpdateRequest(int, boolean)} with a non-null
         *     {@code sideUiId}.
         */
        @Deprecated
        public UiUpdateRequest(@Nullable @SideUiId Integer sideUiId, boolean suppressAnimations) {
            this(
                    sideUiId,
                    suppressAnimations,
                    sideUiId != null ? UpdateReason.SIDE_UI_REQUEST : UpdateReason.UNSPECIFIED);
        }

        /**
         * Constructs a request with an explicit {@link UpdateReason}.
         *
         * <p>Restricted to this package. The {@link UpdateReason}s can describe changes to the
         * environment that the {@link SideUiContainer}s are laid out within, so only the Side UI
         * framework can use this constructor to freely specify an {@link UpdateReason}.
         *
         * @param sideUiId ID of the {@link SideUiContainer} requesting the UI update, or null if
         *     the request is not from a {@link SideUiContainer}.
         * @param suppressAnimations Whether animations should be suppressed during the UI update.
         * @param updateReason What triggered this request.
         */
        @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
        public UiUpdateRequest(
                @Nullable @SideUiId Integer sideUiId,
                boolean suppressAnimations,
                @UpdateReason int updateReason) {
            mSideUiId = sideUiId;
            mSuppressAnimations = suppressAnimations;
            mUpdateReason = updateReason;
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (this == obj) return true;
            if (!(obj instanceof UiUpdateRequest other)) return false;
            return Objects.equals(mSideUiId, other.mSideUiId)
                    && mSuppressAnimations == other.mSuppressAnimations
                    && mUpdateReason == other.mUpdateReason;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mSideUiId, mSuppressAnimations, mUpdateReason);
        }

        @Override
        public String toString() {
            return String.format(
                    Locale.US,
                    "UiUpdateRequest{sideUiId=%s, suppressAnimations=%b, updateReason=%s}",
                    mSideUiId,
                    mSuppressAnimations,
                    updateReasonToString(mUpdateReason));
        }

        /** Returns the name of the given {@link UpdateReason}. */
        private static String updateReasonToString(@UpdateReason int reason) {
            return switch (reason) {
                case UpdateReason.UNSPECIFIED -> "UNSPECIFIED";
                case UpdateReason.SIDE_UI_REQUEST -> "SIDE_UI_REQUEST";
                case UpdateReason.ANDROID_CONFIGURATION_CHANGED -> "ANDROID_CONFIGURATION_CHANGED";
                case UpdateReason.TOP_CONTROLS_HEIGHT_CHANGED -> "TOP_CONTROLS_HEIGHT_CHANGED";
                case UpdateReason.FULL_SCREEN_MODE_ENTERED -> "FULL_SCREEN_MODE_ENTERED";
                case UpdateReason.FULL_SCREEN_MODE_EXITED -> "FULL_SCREEN_MODE_EXITED";
                case UpdateReason.RESIZE_LIVE -> "RESIZE_LIVE";
                case UpdateReason.RESIZE_COMMITTED -> "RESIZE_COMMITTED";
                default -> "UNKNOWN(" + reason + ")";
            };
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
            /**
             * The width reserved by the {@link SideUiContainer}, i.e. the width that the rest of
             * the browser UI (web contents, toolbar, etc.) lays itself out around.
             */
            public final @Px int mReservedWidth;

            /**
             * The width the {@link SideUiContainer}'s {@link android.view.View} is rendered at.
             *
             * <p>This is the same as {@link #mReservedWidth} unless the container overlays other
             * browser UI.
             */
            public final @Px int mRenderedWidth;

            public final @HeightType int mHeightType;

            public SideUiSize(@Px int reservedWidth, @HeightType int heightType) {
                this(reservedWidth, /* renderedWidth= */ reservedWidth, heightType);
            }

            public SideUiSize(
                    @Px int reservedWidth, @Px int renderedWidth, @HeightType int heightType) {
                assert reservedWidth > 0
                                || (reservedWidth == 0 && heightType == HeightType.NOT_APPLICABLE)
                        : "inconsistent width and heightType";
                assert renderedWidth >= reservedWidth
                        : "a container cannot render narrower than the width it reserves";
                assert reservedWidth > 0 || renderedWidth == 0
                        : "a container that reserves no width cannot render an overlay";

                mReservedWidth = reservedWidth;
                mRenderedWidth = renderedWidth;
                mHeightType = heightType;
            }

            @Override
            public boolean equals(@Nullable Object obj) {
                if (!(obj instanceof SideUiSize that)) return false;
                return this.mReservedWidth == that.mReservedWidth
                        && this.mRenderedWidth == that.mRenderedWidth
                        && this.mHeightType == that.mHeightType;
            }

            @Override
            public int hashCode() {
                return Objects.hash(mReservedWidth, mRenderedWidth, mHeightType);
            }

            @Override
            public String toString() {
                return String.format(
                        Locale.ENGLISH,
                        "[reservedWidth: %d, renderedWidth: %d, heightType: %d]",
                        mReservedWidth,
                        mRenderedWidth,
                        mHeightType);
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

        /**
         * Returns the width the container on {@code side} reserves, i.e. the width that the rest of
         * the browser UI lays itself out around.
         */
        public int getReservedWidth(@AnchorSide int side) {
            SideUiSize spec = mSideUiSpecs.get(side);
            return spec != null ? spec.mReservedWidth : 0;
        }

        /**
         * Returns the width the container on {@code side} is rendered at, which is larger than
         * {@link #getReservedWidth} when that container overlays the web contents.
         */
        public int getRenderedWidth(@AnchorSide int side) {
            SideUiSize spec = mSideUiSpecs.get(side);
            return spec != null ? spec.mRenderedWidth : 0;
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
     * @throws IllegalArgumentException if the given sideUiContainer has conflicts with the existing
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
