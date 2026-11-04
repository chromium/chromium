// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.build.annotations.NullMarked;

/** Model representing an entry in the adaptive UI system. */
@NullMarked
public class AdaptiveUiEntry {
    private final @AdaptiveUiScope int mScope;
    private final @AdaptiveUiContainer int mAllowedContainers;

    private AdaptiveUiEntry(
            @AdaptiveUiScope int scope, @AdaptiveUiContainer int allowedContainers) {
        mScope = scope;
        mAllowedContainers = allowedContainers;
    }

    public @AdaptiveUiScope int getScope() {
        return mScope;
    }

    public @AdaptiveUiContainer int getAllowedContainers() {
        return mAllowedContainers;
    }

    /** Builder for {@link AdaptiveUiEntry}. */
    public static class Builder {
        private @AdaptiveUiContainer int mAllowedContainers;
        private @AdaptiveUiScope int mScope = AdaptiveUiScope.TRANSIENT;

        public Builder setScope(@AdaptiveUiScope int scope) {
            mScope = scope;
            return this;
        }

        public Builder addAllowedContainer(@AdaptiveUiContainer int container) {
            mAllowedContainers |= container;
            return this;
        }

        /** Builds the {@link AdaptiveUiEntry}. */
        public AdaptiveUiEntry build() {
            if (mAllowedContainers == 0) {
                throw new IllegalStateException(
                        "At least one allowed container must be specified.");
            }
            return new AdaptiveUiEntry(mScope, mAllowedContainers);
        }
    }
}
