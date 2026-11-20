// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * An entry in the adaptive UI system: a {@link PropertyModel} (built with {@link
 * AdaptiveUiProperties#ALL_KEYS}) plus where it may be shown. The model holds what the entry shows,
 * its callbacks and its container-specific options; this class holds the scope and the allowed
 * containers, which {@link AdaptiveUiController} reads to pick the container.
 */
@NullMarked
public class AdaptiveUiEntry {
    private final @AdaptiveUiScope int mScope;
    private final Set<@AdaptiveUiContainer Integer> mAllowedContainers;
    private final PropertyModel mPropertyModel;

    private AdaptiveUiEntry(
            @AdaptiveUiScope int scope,
            Set<@AdaptiveUiContainer Integer> allowedContainers,
            PropertyModel propertyModel) {
        mScope = scope;
        mAllowedContainers = allowedContainers;
        mPropertyModel = propertyModel;
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

    /** Builder for {@link AdaptiveUiEntry}. */
    public static class Builder {
        private final PropertyModel mPropertyModel;
        private final Set<@AdaptiveUiContainer Integer> mAllowedContainers = new HashSet<>();
        private @AdaptiveUiScope int mScope = AdaptiveUiScope.TRANSIENT;

        /**
         * Creates a builder for an {@link AdaptiveUiEntry}.
         *
         * @param propertyModel The shared {@link PropertyModel} (built with {@link
         *     AdaptiveUiProperties#ALL_KEYS}) describing the entry's header, body, action buttons,
         *     lifecycle callbacks and container-specific options.
         * @throws IllegalArgumentException if {@link AdaptiveUiProperties#SHEET_INITIAL_STATE} is
         *     set to an unsupported state.
         */
        public Builder(PropertyModel propertyModel) {
            @SheetState
            int initialState = propertyModel.get(AdaptiveUiProperties.SHEET_INITIAL_STATE);
            if (initialState != SheetState.FULL && initialState != SheetState.HALF) {
                throw new IllegalArgumentException(
                        "Unsupported initial sheet state: " + initialState);
            }
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
                    mPropertyModel);
        }
    }
}
