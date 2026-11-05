// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Model representing an entry in the adaptive UI system. */
@NullMarked
public class AdaptiveUiEntry {
    private final @AdaptiveUiScope int mScope;
    private final @AdaptiveUiContainer int mAllowedContainers;
    private final PropertyModel mPropertyModel;

    private AdaptiveUiEntry(
            @AdaptiveUiScope int scope,
            @AdaptiveUiContainer int allowedContainers,
            PropertyModel propertyModel) {
        mScope = scope;
        mAllowedContainers = allowedContainers;
        mPropertyModel = propertyModel;
    }

    public @AdaptiveUiScope int getScope() {
        return mScope;
    }

    public @AdaptiveUiContainer int getAllowedContainers() {
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
        private @AdaptiveUiContainer int mAllowedContainers;
        private @AdaptiveUiScope int mScope = AdaptiveUiScope.TRANSIENT;

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
            mAllowedContainers |= container;
            return this;
        }

        /**
         * Builds the {@link AdaptiveUiEntry}.
         *
         * @throws IllegalStateException if no allowed container was set.
         */
        public AdaptiveUiEntry build() {
            if (mAllowedContainers == 0) {
                throw new IllegalStateException(
                        "At least one allowed container must be specified.");
            }
            return new AdaptiveUiEntry(mScope, mAllowedContainers, mPropertyModel);
        }
    }
}
