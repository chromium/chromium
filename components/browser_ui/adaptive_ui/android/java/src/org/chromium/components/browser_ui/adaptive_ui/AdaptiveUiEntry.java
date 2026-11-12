// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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

    private AdaptiveUiEntry(
            @AdaptiveUiScope int scope,
            Set<@AdaptiveUiContainer Integer> allowedContainers,
            PropertyModel propertyModel,
            @Nullable AdaptiveUiObserver observer) {
        mScope = scope;
        mAllowedContainers = allowedContainers;
        mPropertyModel = propertyModel;
        mObserver = observer;
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

    /** Builder for {@link AdaptiveUiEntry}. */
    public static class Builder {
        private final PropertyModel mPropertyModel;
        private final Set<@AdaptiveUiContainer Integer> mAllowedContainers = new HashSet<>();
        private @AdaptiveUiScope int mScope = AdaptiveUiScope.TRANSIENT;
        private @Nullable AdaptiveUiObserver mObserver;

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
                    mObserver);
        }
    }
}
