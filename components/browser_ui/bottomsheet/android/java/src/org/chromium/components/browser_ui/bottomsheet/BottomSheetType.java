// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * Encapsulates the core behavioral characteristics of a {@link BottomSheetContent}.
 *
 * <p>Used by {@link BottomSheetController} to determine prioritization, suppression, and lifecycle
 * behavior when resolving conflicts between sheets.
 */
@NullMarked
public final class BottomSheetType {
    /**
     * Predefined, standard {@link BottomSheetType} instances for common bottom sheet use cases in
     * Chrome.
     */
    public static final class Type {
        private Type() {}

        /**
         * In-Product Help (IPH) promos and contextual tips.
         *
         * <p>Triggered passively based on browser state, displayed without a scrim (non-modal), and
         * suppressible when other content requests to be shown.
         */
        public static final BottomSheetType IPH =
                new BottomSheetType.Builder()
                        .setUserInitiated(false)
                        .setModal(false)
                        .setSuppressible(true)
                        .build();
    }

    private final boolean mIsUserInitiated;
    private final boolean mIsModal;
    private final boolean mIsSuppressible;
    private final boolean mIsPersistent;
    private final @UserCriticalFeature int mUserCriticalFeature;

    private BottomSheetType(Builder builder) {
        mIsUserInitiated = builder.mIsUserInitiated;
        mIsModal = builder.mIsModal;
        mIsSuppressible = builder.mIsSuppressible;
        mIsPersistent = builder.mIsPersistent;
        mUserCriticalFeature = builder.mUserCriticalFeature;
    }

    /**
     * @return {@code true} if triggered directly by explicit user action (e.g. preview link);
     *     {@code false} if triggered passively by browser state (e.g. promo, autofill popup).
     */
    public boolean isUserInitiated() {
        return mIsUserInitiated;
    }

    /**
     * @return {@code true} if the sheet is modal, meaning the bottom sheet will create and manage
     *     its own background scrim blocking interaction with the web page; {@code false} if content
     *     behind the sheet remains interactable.
     */
    public boolean isModal() {
        return mIsModal;
    }

    /**
     * @return {@code true} if the sheet explicitly permits being suppressed by other incoming
     *     sheets regardless of other priority attributes; {@code false} otherwise.
     */
    public boolean isSuppressible() {
        return mIsSuppressible;
    }

    /**
     * @return {@code true} if the sheet is designed to remain with the user for a longer period of
     *     time (e.g. persistent tools, co-browsing); {@code false} if it is a one-off sheet used
     *     and destroyed.
     */
    public boolean isPersistent() {
        return mIsPersistent;
    }

    /**
     * @return {@code true} if this sheet represents an approved user-critical feature.
     */
    public boolean isUserCritical() {
        return mUserCriticalFeature != UserCriticalFeature.NONE;
    }

    /**
     * Evaluates whether this (incoming) sheet type has sufficient precedence to suppress or
     * supersede a {@code currentlyShowing} sheet type.
     *
     * <p>The precedence is evaluated in the following order:
     *
     * <ol>
     *   <li><strong>Equality:</strong> If both types are equal, the incoming sheet does not
     *       supersede (returns {@code false}).
     *   <li><strong>Suppressibility:</strong> If the current sheet is marked as suppressible
     *       ({@link #isSuppressible()}), it is always superseded by any incoming sheet (returns
     *       {@code true}).
     *   <li><strong>User-Critical:</strong> If only one sheet is user-critical ({@link
     *       #isUserCritical()}), the user-critical sheet takes precedence. A non-critical sheet
     *       cannot supersede a critical sheet, while a critical sheet always supersedes a
     *       non-critical sheet.
     *   <li><strong>User-Initiated:</strong> An incoming user-initiated sheet ({@link
     *       #isUserInitiated()}) always supersedes the current sheet (whether current is passive or
     *       user-initiated). A passive incoming sheet cannot supersede an active user-initiated
     *       sheet.
     *   <li><strong>Persistence:</strong> If only one sheet is persistent ({@link
     *       #isPersistent()}), the persistent sheet takes precedence (returns {@code
     *       this.isPersistent()}).
     *   <li><strong>Modal:</strong> An incoming modal sheet ({@link #isModal()}) supersedes a
     *       non-modal or modal current sheet. A non-modal incoming sheet cannot supersede the
     *       current sheet.
     * </ol>
     *
     * @param currentlyShowing The type of the bottom sheet currently on screen.
     * @return {@code true} if this incoming sheet should supersede the current sheet; {@code false}
     *     if the current sheet should be kept and the incoming sheet queued/rejected.
     */
    public boolean canSupersede(BottomSheetType currentlyShowing) {
        if (this.equals(currentlyShowing)) {
            return false;
        }

        if (currentlyShowing.isSuppressible()) {
            return true;
        }

        if (this.isUserCritical() != currentlyShowing.isUserCritical()) {
            return this.isUserCritical();
        }

        if (this.isUserInitiated()) {
            return true;
        }
        if (currentlyShowing.isUserInitiated()) {
            return false;
        }

        if (this.isPersistent() != currentlyShowing.isPersistent()) {
            return this.isPersistent();
        }

        return this.isModal();
    }

    /**
     * Compares two {@link BottomSheetType} instances for precedence in a priority queue.
     *
     * @return Negative if {@code a} has higher precedence than {@code b}, positive if {@code b} has
     *     higher precedence than {@code a}, or 0 if they have equal precedence.
     */
    public static int compare(BottomSheetType a, BottomSheetType b) {
        if (a.canSupersede(b)) return -1;
        if (b.canSupersede(a)) return 1;
        return 0;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof BottomSheetType)) return false;
        BottomSheetType other = (BottomSheetType) obj;
        return mIsUserInitiated == other.mIsUserInitiated
                && mIsModal == other.mIsModal
                && mIsSuppressible == other.mIsSuppressible
                && mIsPersistent == other.mIsPersistent
                && mUserCriticalFeature == other.mUserCriticalFeature;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mIsUserInitiated, mIsModal, mIsSuppressible, mIsPersistent, mUserCriticalFeature);
    }

    @Override
    public String toString() {
        return "BottomSheetType{"
                + "userInitiated="
                + mIsUserInitiated
                + ", modal="
                + mIsModal
                + ", suppressible="
                + mIsSuppressible
                + ", persistent="
                + mIsPersistent
                + ", userCritical="
                + mUserCriticalFeature
                + '}';
    }

    /** Builder for constructing {@link BottomSheetType} instances. */
    public static final class Builder {
        private boolean mIsUserInitiated;
        private boolean mIsModal = true;
        private boolean mIsSuppressible;
        private boolean mIsPersistent;
        private @UserCriticalFeature int mUserCriticalFeature = UserCriticalFeature.NONE;

        /**
         * Sets whether the sheet is triggered directly by explicit user action (e.g. preview link)
         * rather than passively by browser state (e.g. promo, autofill popup).
         *
         * @param isUserInitiated Whether the sheet is user-initiated. Defaults to {@code false}.
         * @return This builder.
         */
        public Builder setUserInitiated(boolean isUserInitiated) {
            mIsUserInitiated = isUserInitiated;
            return this;
        }

        /**
         * Sets whether the sheet includes a background scrim blocking interaction with the web
         * page.
         *
         * <p>If modal is {@code true}, the bottom sheet will create and manage its own background
         * scrim. If {@code false}, no scrim is shown and content behind the sheet remains
         * interactable.
         *
         * @param isModal Whether the sheet is modal. Defaults to {@code true}.
         * @return This builder.
         */
        public Builder setModal(boolean isModal) {
            mIsModal = isModal;
            return this;
        }

        /**
         * Sets whether the sheet explicitly permits being suppressed by other incoming sheets
         * regardless of other priority attributes.
         *
         * @param isSuppressible Whether the sheet is suppressible. Defaults to {@code false}.
         * @return This builder.
         */
        public Builder setSuppressible(boolean isSuppressible) {
            mIsSuppressible = isSuppressible;
            return this;
        }

        /**
         * Sets whether the sheet is designed to remain with the user for a longer period of time
         * rather than being a one-off sheet that is used and destroyed.
         *
         * @param isPersistent Whether the sheet is persistent. Defaults to {@code false}.
         * @return This builder.
         */
        public Builder setPersistent(boolean isPersistent) {
            mIsPersistent = isPersistent;
            return this;
        }

        /**
         * Marks this sheet as user-critical.
         *
         * <p>Requires an approved {@link UserCriticalFeature} constant. Adding new features to
         * {@link UserCriticalFeature} requires BottomSheet OWNERS approval.
         *
         * @param feature The approved {@link UserCriticalFeature} constant. Defaults to {@link
         *     UserCriticalFeature#NONE}.
         * @return This builder.
         */
        public Builder setUserCritical(@UserCriticalFeature int feature) {
            mUserCriticalFeature = feature;
            return this;
        }

        /** Builds and returns the configured {@link BottomSheetType}. */
        public BottomSheetType build() {
            return new BottomSheetType(this);
        }
    }
}
