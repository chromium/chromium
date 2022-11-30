// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import androidx.annotation.Nullable;

/**
 * Provides multiple types of renderers to surfaces that want to render an
 * external surface. Each renderer will reuse the same dependencies (hence
 * "Scope") but each call to provideFoo will return a new renderer, so that a
 * single surface can support multiple rendered views.
 */
public interface SurfaceScope {
    @Nullable
    default HybridListRenderer provideListRenderer() {
        return null;
    }

    @Nullable
    default SurfaceRenderer provideSurfaceRenderer() {
        return null;
    }

    default void replaceDataStoreEntry(String key, byte[] data) {}
    default void removeDataStoreEntry(String key) {}

    /**
     * Return the FeedLaunchReliabilityLogger associated with the surface, creating it if it
     * doesn't exist.
     * @return The surface's FeedLaunchReliabilityLogger instance.
     */
    default FeedLaunchReliabilityLogger getFeedLaunchReliabilityLogger() {
        return new FeedLaunchReliabilityLogger() {};
    }
}
