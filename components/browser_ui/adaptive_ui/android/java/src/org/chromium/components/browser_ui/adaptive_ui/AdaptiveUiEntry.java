// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Model representing an entry in the adaptive UI system. */
@NullMarked
public class AdaptiveUiEntry {
    private final @AdaptiveUiScope int mScope;
    private final Set<@AdaptiveUiContainer Integer> mAllowedContainers;
    private final PropertyModel mPropertyModel;
    private final @Nullable AdaptiveUiObserver mObserver;
    private final boolean mIsSheetUserInitiated;
    private final @UserCriticalFeature int mSheetUserCriticalFeature;
    private final @SheetState int mSheetInitialState;

    private AdaptiveUiEntry(
            @AdaptiveUiScope int scope,
            Set<@AdaptiveUiContainer Integer> allowedContainers,
            PropertyModel propertyModel,
            @Nullable AdaptiveUiObserver observer,
            boolean isSheetUserInitiated,
            @UserCriticalFeature int sheetUserCriticalFeature,
            @SheetState int sheetInitialState) {
        mScope = scope;
        mAllowedContainers = allowedContainers;
        mPropertyModel = propertyModel;
        mObserver = observer;
        mIsSheetUserInitiated = isSheetUserInitiated;
        mSheetUserCriticalFeature = sheetUserCriticalFeature;
        mSheetInitialState = sheetInitialState;
    }

    public @AdaptiveUiScope int getScope() {
        return mScope;
    }

    /** Returns the containers this entry may be shown in; never empty. */
    public Set<@AdaptiveUiContainer Integer> getAllowedContainers() {
        return mAllowedContainers;
    }

    /**
     * Returns the shared {@link PropertyModel} (built with {@link AdaptiveUiProperties#ALL_KEYS}).
     */
    public PropertyModel getPropertyModel() {
        return mPropertyModel;
    }

    /** Returns the observer set via {@link Builder#setObserver}, or null if none was set. */
    public @Nullable AdaptiveUiObserver getObserver() {
        return mObserver;
    }

    /**
     * Optional, bottom sheet only: whether the sheet is triggered directly by an explicit user
     * action rather than passively by browser state. Defaults to {@code false}.
     */
    public boolean isSheetUserInitiated() {
        return mIsSheetUserInitiated;
    }

    /**
     * Optional, bottom sheet only: the approved {@link UserCriticalFeature} for sheet precedence,
     * or {@link UserCriticalFeature#NONE} (default).
     */
    public @UserCriticalFeature int getSheetUserCriticalFeature() {
        return mSheetUserCriticalFeature;
    }

    /**
     * Optional, bottom sheet only: the {@link SheetState} the sheet opens in, either {@link
     * SheetState#FULL} (default) or {@link SheetState#HALF}.
     */
    public @SheetState int getSheetInitialState() {
        return mSheetInitialState;
    }

    /** Builder for {@link AdaptiveUiEntry}. */
    public static class Builder {
        private final PropertyModel mPropertyModel;
        private final Set<@AdaptiveUiContainer Integer> mAllowedContainers = new HashSet<>();
        private @AdaptiveUiScope int mScope = AdaptiveUiScope.TRANSIENT;
        private @Nullable AdaptiveUiObserver mObserver;
        private boolean mIsSheetUserInitiated;
        private @UserCriticalFeature int mSheetUserCriticalFeature = UserCriticalFeature.NONE;
        private @SheetState int mSheetInitialState = SheetState.FULL;

        /**
         * Creates a builder for an {@link AdaptiveUiEntry}.
         *
         * @param propertyModel The shared {@link PropertyModel} (built with {@link
         *     AdaptiveUiProperties#ALL_KEYS}) describing the entry's header, body, and action
         *     buttons.
         */
        public Builder(PropertyModel propertyModel) {
            mPropertyModel = propertyModel;
        }

        public Builder setScope(@AdaptiveUiScope int scope) {
            mScope = scope;
            return this;
        }

        public Builder addAllowedContainer(@AdaptiveUiContainer int container) {
            mAllowedContainers.add(container);
            return this;
        }

        /**
         * Sets the observer that receives this entry's lifecycle callbacks.
         *
         * @param observer The observer to notify.
         */
        public Builder setObserver(AdaptiveUiObserver observer) {
            mObserver = observer;
            return this;
        }

        /**
         * Optional, bottom sheet only: sets whether the sheet is triggered directly by an explicit
         * user action rather than passively by browser state. Used for bottom sheet precedence.
         *
         * @param isUserInitiated Whether the sheet is user-initiated. Defaults to {@code false}.
         */
        public Builder setSheetUserInitiated(boolean isUserInitiated) {
            mIsSheetUserInitiated = isUserInitiated;
            return this;
        }

        /**
         * Optional, bottom sheet only: sets an approved {@link UserCriticalFeature} constant for
         * top bottom sheet precedence.
         *
         * @param feature The approved {@link UserCriticalFeature} constant. Defaults to {@link
         *     UserCriticalFeature#NONE}.
         */
        public Builder setSheetUserCritical(@UserCriticalFeature int feature) {
            mSheetUserCriticalFeature = feature;
            return this;
        }

        /**
         * Optional, bottom sheet only: sets the {@link SheetState} the sheet opens in.
         *
         * <p>{@link SheetState#FULL} (default) sizes the sheet to its content. {@link
         * SheetState#HALF} opens the sheet at the half height, and the user can drag it to the full
         * height. The half state is skipped on small screens, where the sheet opens at the full
         * height. A half sheet's heights can be set with {@link
         * AdaptiveUiProperties#SHEET_HALF_HEIGHT_RATIO} and {@link
         * AdaptiveUiProperties#SHEET_FULL_HEIGHT_RATIO}.
         *
         * @param state {@link SheetState#FULL} or {@link SheetState#HALF}.
         * @throws IllegalArgumentException for any other {@link SheetState}.
         */
        public Builder setSheetInitialState(@SheetState int state) {
            if (state != SheetState.FULL && state != SheetState.HALF) {
                throw new IllegalArgumentException("Unsupported initial sheet state: " + state);
            }
            mSheetInitialState = state;
            return this;
        }

        /**
         * Builds the {@link AdaptiveUiEntry}.
         *
         * @throws IllegalStateException if no allowed container was set.
         */
        public AdaptiveUiEntry build() {
            if (mAllowedContainers.isEmpty()) {
                throw new IllegalStateException(
                        "At least one allowed container must be specified.");
            }
            return new AdaptiveUiEntry(
                    mScope,
                    Collections.unmodifiableSet(new HashSet<>(mAllowedContainers)),
                    mPropertyModel,
                    mObserver,
                    mIsSheetUserInitiated,
                    mSheetUserCriticalFeature,
                    mSheetInitialState);
        }
    }
}
